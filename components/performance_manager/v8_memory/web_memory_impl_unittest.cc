// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "components/performance_manager/v8_memory/v8_memory_test_helpers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager::v8_memory {

using WebMemoryImplPMTest = V8MemoryPerformanceManagerTestHarness;

class WebMemoryImplTest : public WebMemoryTestHarness {
 protected:
  void MeasureAndVerify(
      FrameNodeImpl* frame,
      base::flat_map<std::string, std::optional<base::ByteCount>> expected);
};

class FakeSecurityChecker : public WebMeasureMemorySecurityChecker {
 public:
  explicit FakeSecurityChecker(bool allowed) : allowed_(allowed) {}

  bool IsMeasureMemoryAllowed(const FrameNode* frame_node) const override {
    return allowed_;
  }

 private:
  bool allowed_;
};

void WebMemoryImplTest::MeasureAndVerify(
    FrameNodeImpl* frame,
    base::flat_map<std::string, std::optional<base::ByteCount>> expected) {
  bool measurement_done = false;
  WebMemoryMeasurer web_memory(
      frame->GetFrameToken(),
      V8DetailedMemoryRequest::MeasurementMode::kDefault,
      base::BindLambdaForTesting([&measurement_done, &expected](
                                     mojom::WebMemoryMeasurementPtr result) {
        base::flat_map<std::string, std::optional<base::ByteCount>> actual;
        for (const auto& entry : result->breakdown) {
          EXPECT_EQ(1u, entry->attribution.size());
          std::string attribution_tag =
              (mojom::WebMemoryAttribution::Scope::kWindow ==
               entry->attribution[0]->scope)
                  ? *entry->attribution[0]->url
                  : *entry->attribution[0]->src;
          actual[attribution_tag] =
              entry->memory.has_value() ? entry->memory : std::nullopt;
        }
        EXPECT_EQ(expected, actual);
        measurement_done = true;
      }));
  V8DetailedMemoryProcessData process_data;
  web_memory.MeasurementComplete(process_node(), &process_data);
  EXPECT_TRUE(measurement_done);
}

TEST_F(WebMemoryImplTest, MeasurerIncludesSameOriginRelatedFrames) {
  auto* main = AddFrameNode("http://foo.com/", base::ByteCount(10));

  AddFrameNode("http://foo.com/iframe", base::ByteCount(20), main);

  MeasureAndVerify(main, {
                             {"http://foo.com/", base::ByteCount(10)},
                             {"http://foo.com/iframe", base::ByteCount(20)},
                         });
}

TEST_F(WebMemoryImplTest, MeasurerIncludesCrossOriginFrames) {
  auto* main = AddFrameNode("http://foo.com", base::ByteCount(10));

  AddFrameNode("http://bar.com/iframe", base::ByteCount(20), main, "bar_id",
               "http://bar.com/iframe_src");

  MeasureAndVerify(main, {{"http://foo.com/", base::ByteCount(10)},
                          {
                              "http://bar.com/iframe_src",
                              base::ByteCount(20),
                          }});
}

TEST_F(WebMemoryImplTest, MeasurerSkipsCrossBrowserContextGroupFrames) {
  auto* main = AddFrameNode("http://foo.com", base::ByteCount(10));

  AddCrossBrowsingInstanceFrameNode("http://foo.com/unrelated",
                                    base::ByteCount(20));

  MeasureAndVerify(main, {{"http://foo.com/", base::ByteCount(10)}});
}

TEST_F(WebMemoryImplPMTest, WebMeasureMemory) {
  blink::LocalFrameToken frame_token =
      blink::LocalFrameToken(main_frame()->GetFrameToken());

  // Call WebMeasureMemory on the performance manager sequence and verify that
  // the result matches the data provided by the mock reporter.
  base::RunLoop run_loop;
  auto measurement_callback =
      base::BindLambdaForTesting([&](mojom::WebMemoryMeasurementPtr result) {
        EXPECT_EQ(1u, result->breakdown.size());
        const auto& entry = result->breakdown[0];
        EXPECT_EQ(1u, entry->attribution.size());
        EXPECT_EQ(kMainFrameUrl, *(entry->attribution[0]->url));
        ASSERT_TRUE(entry->memory.has_value());
        EXPECT_EQ(base::ByteCount(1001), entry->memory);
        run_loop.Quit();
      });
  auto bad_message_callback =
      base::BindLambdaForTesting([&](std::string_view error) {
        ADD_FAILURE() << error;
        run_loop.Quit();
      });

  base::WeakPtr<FrameNode> frame_node_wrapper =
      PerformanceManager::GetFrameNodeForRenderFrameHost(main_frame());
  ASSERT_TRUE(frame_node_wrapper);
  FrameNode* frame_node = frame_node_wrapper.get();
  WebMeasureMemory(frame_node, mojom::WebMemoryMeasurement::Mode::kDefault,
                   std::make_unique<FakeSecurityChecker>(true),
                   std::move(measurement_callback),
                   std::move(bad_message_callback));

  // Set up and bind the mock reporter.
  MockV8DetailedMemoryReporter mock_reporter;
  {
    auto data = NewPerProcessV8MemoryUsage(1);
    AddIsolateMemoryUsage(frame_token, base::ByteCount(1001),
                          data->isolates[0].get());
    ExpectBindAndRespondToQuery(&mock_reporter, std::move(data),
                                main_process_id());
  }

  // Finally, run all tasks to verify that the memory measurement callback
  // is actually invoked. The test will time out if not.
  run_loop.Run();
}

TEST_F(WebMemoryImplPMTest, MeasurementInterrupted) {
  CreateCrossProcessChildFrame();

  blink::LocalFrameToken frame_token =
      blink::LocalFrameToken(child_frame()->GetFrameToken());

  // Call WebMeasureMemory on the performance manager sequence but delete the
  // process being measured before the result arrives.
  auto measurement_callback =
      base::BindOnce([](mojom::WebMemoryMeasurementPtr result) {
        FAIL() << "Measurement callback ran unexpectedly";
      });
  auto bad_message_callback =
      base::BindOnce([](std::string_view error) { FAIL() << error; });

  base::WeakPtr<FrameNode> frame_node_wrapper =
      PerformanceManager::GetFrameNodeForRenderFrameHost(child_frame());
  ASSERT_TRUE(frame_node_wrapper);
  FrameNode* frame_node = frame_node_wrapper.get();
  WebMeasureMemory(frame_node, mojom::WebMemoryMeasurement::Mode::kDefault,
                   std::make_unique<FakeSecurityChecker>(true),
                   std::move(measurement_callback),
                   std::move(bad_message_callback));

  // Set up and bind the mock reporter.
  MockV8DetailedMemoryReporter mock_reporter;
  {
    ::testing::InSequence seq;
    ExpectBindReceiver(&mock_reporter, child_process_id());

    auto data = NewPerProcessV8MemoryUsage(1);
    AddIsolateMemoryUsage(frame_token, base::ByteCount(1001),
                          data->isolates[0].get());
    ExpectQueryAndDelayReply(&mock_reporter, base::Seconds(10),
                             std::move(data));
  }

  // Verify that requests are sent but reply is not yet received.
  task_environment()->FastForwardBy(base::Seconds(5));
  ::testing::Mock::VerifyAndClearExpectations(&mock_reporter);

  // Remove the child frame, which will destroy the child process.
  content::RenderFrameHostTester::For(child_frame())->Detach();

  // Advance until the reply is expected to make sure nothing explodes.
  task_environment()->FastForwardBy(base::Seconds(5));
}

TEST_F(WebMemoryImplPMTest, MeasurementDisallowed) {
  // Call WebMeasureMemory on the performance manager sequence but expect the
  // mojo ReportBadMessage callback to be called.
  base::RunLoop run_loop;
  auto measurement_callback =
      base::BindLambdaForTesting([&](mojom::WebMemoryMeasurementPtr result) {
        ADD_FAILURE() << "Measurement callback ran unexpectedly.";
        run_loop.Quit();
      });
  auto bad_message_callback =
      base::BindLambdaForTesting([&](std::string_view error) {
        SUCCEED() << error;
        run_loop.Quit();
      });

  base::WeakPtr<FrameNode> frame_node_wrapper =
      PerformanceManager::GetFrameNodeForRenderFrameHost(main_frame());
  ASSERT_TRUE(frame_node_wrapper);
  FrameNode* frame_node = frame_node_wrapper.get();
  WebMeasureMemory(frame_node, mojom::WebMemoryMeasurement::Mode::kDefault,
                   std::make_unique<FakeSecurityChecker>(false),
                   std::move(measurement_callback),
                   std::move(bad_message_callback));

  run_loop.Run();
}

}  // namespace performance_manager::v8_memory
