// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_MEMORY_TEST_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_MEMORY_TEST_HELPERS_H_

#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/performance_manager/v8_detailed_memory_reporter.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace performance_manager {
namespace v8_memory {

// A fake implementation of the mojo interface that reports memory measurement
// results.
class LenientMockV8DetailedMemoryReporter
    : public blink::mojom::V8DetailedMemoryReporter {
 public:
  LenientMockV8DetailedMemoryReporter();
  ~LenientMockV8DetailedMemoryReporter() override;

  LenientMockV8DetailedMemoryReporter(
      const LenientMockV8DetailedMemoryReporter& other) = delete;
  LenientMockV8DetailedMemoryReporter operator=(
      const LenientMockV8DetailedMemoryReporter& other) = delete;

  MOCK_METHOD(void,
              GetV8MemoryUsage,
              (Mode mode, GetV8MemoryUsageCallback callback),
              (override));

  void Bind(mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
                pending_receiver);

 private:
  mojo::Receiver<blink::mojom::V8DetailedMemoryReporter> receiver_{this};
};

using MockV8DetailedMemoryReporter =
    ::testing::StrictMock<LenientMockV8DetailedMemoryReporter>;

// The mode enum used in test expectations.
using ExpectedMode = MockV8DetailedMemoryReporter::Mode;

// A base class with helper functions to set up test expectations and fake mojo
// connections for V8 memory tests. This can be composed with GraphTestHarness
// or PerformanceManagerTestHarness to make a full test environment.
class V8MemoryTestBase {
 public:
  // A default process ID to use in tests.
  static constexpr RenderProcessHostId kTestProcessID =
      RenderProcessHostId(0xFAB);

  V8MemoryTestBase();
  virtual ~V8MemoryTestBase();

  // Adaptor that calls GetMainThreadTaskRunner for the test harness's task
  // environment.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetMainThreadTaskRunner() = 0;

 protected:
  // Simulate a renderer process reporting the given |data| over the
  // mojom::V8DetailedMemoryReporter interface using |callback|.
  void ReplyWithData(
      blink::mojom::PerProcessV8MemoryUsagePtr data,
      MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback);

  // Simulate a renderer process reporting the given |data| over the
  // mojom::V8DetailedMemoryReporter interface using |callback|, after a delay
  // of |delay|.
  void DelayedReplyWithData(
      const base::TimeDelta& delay,
      blink::mojom::PerProcessV8MemoryUsagePtr data,
      MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback);

  // Add a test expectation that the GetV8MemoryUsage method of |mock_reporter|
  // will be called with the mode parameter equal to |expected_mode|. When the
  // method is called, |responder| will be invoked to call the method's
  // response callback. (ReplyWithData and DelayedReplyWithData are examples of
  // methods that could be bound into |responder| callbacks.)
  void ExpectQuery(
      MockV8DetailedMemoryReporter* mock_reporter,
      base::RepeatingCallback<
          void(MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback)>
          responder,
      ExpectedMode expected_mode = ExpectedMode::DEFAULT);

  // Add a test expectation that the GetV8MemoryUsage method of |mock_reporter|
  // will be called with the mode parameter equal to |expected_mode|. When the
  // method is called, its response callback will be called with |data|.
  void ExpectQueryAndReply(MockV8DetailedMemoryReporter* mock_reporter,
                           blink::mojom::PerProcessV8MemoryUsagePtr data,
                           ExpectedMode expected_mode = ExpectedMode::DEFAULT);

  // Add a test expectation that the GetV8MemoryUsage method of |mock_reporter|
  // will be called with the mode parameter equal to |expected_mode|. When the
  // method is called, its response callback will be called with |data| after a
  // delay of |delay|.
  void ExpectQueryAndDelayReply(
      MockV8DetailedMemoryReporter* mock_reporter,
      const base::TimeDelta& delay,
      blink::mojom::PerProcessV8MemoryUsagePtr data,
      ExpectedMode expected_mode = ExpectedMode::DEFAULT);

  // Add a test expectation that
  // BindReceiverWithProxyHost will be called with the proxy parameter having ID
  // |expected_process_id|.
  void ExpectBindReceiver(
      MockV8DetailedMemoryReporter* mock_reporter,
      RenderProcessHostId expected_process_id = kTestProcessID);

  // Add test expectations that two methods will be called in sequence:
  // BindReceiverWithProxyHost (as in ExpectBindReceiver) and GetV8MemoryUsage
  // (as in ExpectQueryAndReply). This is a useful shorthand because the
  // receiver is always bound just before sending the first request to a
  // process.
  void ExpectBindAndRespondToQuery(
      MockV8DetailedMemoryReporter* mock_reporter,
      blink::mojom::PerProcessV8MemoryUsagePtr data,
      RenderProcessHostId expected_process_id = kTestProcessID,
      ExpectedMode expected_mode = ExpectedMode::DEFAULT);

  // A mock method that will be called the first time a memory measurement is
  // requested for a process.
  MOCK_METHOD(void,
              BindReceiverWithProxyHost,
              (mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
                   pending_receiver,
               RenderProcessHostProxy proxy),
              (const));

  // The last time one of the query expectations installed by the Expect*
  // methods was fulfilled.
  base::TimeTicks last_query_time() const { return last_query_time_; }

 private:
  // Invokes BindReceiverWithProxyHost on the main sequence.
  void BindReceiverOnMainSequence(
      mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
          pending_receiver,
      RenderProcessHostProxy proxy);

  // A callback that will live as long as |this| does. Will be installed with
  // SetBindV8DetailedMemoryReporterCallbackForTesting.
  internal::BindV8DetailedMemoryReporterCallback bind_callback_;

  base::TimeTicks last_query_time_;
};

// A PerformanceManagerTestHarness that exposes the helpers from
// V8MemoryTestBase and sets up some frames whose memory can be measured.
class V8MemoryPerformanceManagerTestHarness
    : public PerformanceManagerTestHarness,
      public V8MemoryTestBase {
 public:
  V8MemoryPerformanceManagerTestHarness();
  ~V8MemoryPerformanceManagerTestHarness() override;

  static constexpr char kMainFrameUrl[] = "http://a.com/";
  static constexpr char kChildFrameUrl[] = "http://b.com/";

  void SetUp() override;

  // The main frame, which always exists.
  content::RenderFrameHost* main_frame() const { return main_frame_; }

  // The ID of the main frame's renderer process.
  RenderProcessHostId main_process_id() const { return main_process_id_; }

  // A cross-process child frame. Will return nullptr unless
  // CreateCrossProcessChildFrame is called.
  content::RenderFrameHost* child_frame() const { return child_frame_; }

  // The ID of the child frame's renderer process. Returns null if the child
  // frame does not exist.
  RenderProcessHostId child_process_id() const { return child_process_id_; }

  // Creates a child frame that has its own renderer process. After calling
  // this the frame can be accessed with child_frame() and child_process_id().
  void CreateCrossProcessChildFrame();

  // V8MemoryTestBase implementation.
  scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner()
      override;

 private:
  content::RenderFrameHost* main_frame_ = nullptr;
  content::RenderFrameHost* child_frame_ = nullptr;
  RenderProcessHostId main_process_id_;
  RenderProcessHostId child_process_id_;
};

// Returns a new mojom::PerProcessV8MemoryUsage struct with
// |number_of_isolates| empty isolates.
blink::mojom::PerProcessV8MemoryUsagePtr NewPerProcessV8MemoryUsage(
    size_t number_of_isolates);

// Finds the PerContextV8MemoryUsage in |isolate| whose token is |frame_token|,
// or creates it if it does not exist, and sets its bytes_used to |bytes_used|.
void AddIsolateMemoryUsage(const blink::LocalFrameToken& frame_token,
                           uint64_t bytes_used,
                           blink::mojom::PerIsolateV8MemoryUsage* isolate);

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_MEMORY_TEST_HELPERS_H_
