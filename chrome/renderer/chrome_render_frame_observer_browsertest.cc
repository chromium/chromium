// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <string>
#include <tuple>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/optimization_guide/content/renderer/page_text_agent.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace {

class FakeContentTranslateDriver
    : public translate::mojom::ContentTranslateDriver {
 public:
  FakeContentTranslateDriver() = default;
  ~FakeContentTranslateDriver() override = default;

  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this, mojo::PendingReceiver<translate::mojom::ContentTranslateDriver>(
                  std::move(handle)));
  }

  // translate::mojom::ContentTranslateDriver implementation.
  void RegisterPage(
      mojo::PendingRemote<translate::mojom::TranslateAgent> translate_agent,
      const translate::LanguageDetectionDetails& details,
      bool page_level_translation_criteria_met) override {
    register_page_count_ += 1;
    page_level_translation_criteria_met_ = page_level_translation_criteria_met;
  }

  int register_page_count_ = 0;
  bool page_level_translation_criteria_met_ = false;

 private:
  mojo::ReceiverSet<translate::mojom::ContentTranslateDriver> receivers_;
};

class TestOptGuideConsumer
    : public optimization_guide::mojom::PageTextConsumer {
 public:
  TestOptGuideConsumer() = default;
  ~TestOptGuideConsumer() override = default;

  std::u16string text() const { return base::StrCat(chunks_); }
  bool on_chunks_end_called() const { return on_chunks_end_called_; }
  size_t num_chunks() const { return chunks_.size(); }

  void Bind(mojo::PendingReceiver<optimization_guide::mojom::PageTextConsumer>
                pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  // optimization_guide::mojom::PageTextConsumer:
  void OnTextDumpChunk(const std::u16string& chunk) override {
    ASSERT_FALSE(on_chunks_end_called_);
    chunks_.push_back(chunk);
  }

  void OnChunksEnd() override { on_chunks_end_called_ = true; }

 private:
  mojo::Receiver<optimization_guide::mojom::PageTextConsumer> receiver_{this};
  std::vector<std::u16string> chunks_;
  bool on_chunks_end_called_ = false;
};

}  // namespace


class ChromeRenderFrameObserverTest : public ChromeRenderViewTest {
 public:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        translate::mojom::ContentTranslateDriver::Name_,
        base::BindRepeating(&FakeContentTranslateDriver::BindHandle,
                            base::Unretained(&fake_translate_driver_)));
  }

  void TearDown() override {
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        translate::mojom::ContentTranslateDriver::Name_, {});

    ChromeRenderViewTest::TearDown();
  }

  content::RenderFrame* render_frame() { return GetMainRenderFrame(); }

 protected:
  FakeContentTranslateDriver fake_translate_driver_;
};

class ChromeRenderFrameObserverWithBenchmarkingTest
    : public ChromeRenderFrameObserverTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kEnableBenchmarkingApi);
    ChromeRenderFrameObserverTest::SetUp();
  }
};

TEST_F(ChromeRenderFrameObserverTest, CapturePageTextCalled) {
  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 1);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest, CapturePageTextNotCalledForSubframe) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<!DOCTYPE html><body>"
      "This is a main document"
      "<iframe srcdoc=\"This a document in an iframe.\">"
      "</body>");


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 1);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForUpcomingNavigation) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<html><head>"
      "<meta http-equiv=\"refresh\" content=\"1\"></head>"
      "<body>foo</body></html>");


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 0);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForViewSourceMode) {
  base::HistogramTester histogram_tester;
  render_frame()->GetWebFrame()->EnableViewSourceMode(true);

  LoadHTML("<html><body>foo</body></html>");


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 0);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForUnreachableURL) {
  base::HistogramTester histogram_tester;

  render_frame()->LoadHTMLStringForTesting("<html><body>foo</body></html>",
                                           GURL("data:text/html,"), "UTF-8",
                                           GURL("http://unreachable.com"),
                                           /*replace_current_item=*/false);


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 0);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForNoStatePrefetch) {
  base::HistogramTester histogram_tester;

  prerender::NoStatePrefetchHelper helper(render_frame(), "");

  LoadHTML("<html><body>foo</body></html>");


  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_translate_driver_.register_page_count_, 0);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);
}

TEST_F(ChromeRenderFrameObserverTest, OptGuideGetsText) {
  optimization_guide::PageTextAgent* agent =
      optimization_guide::PageTextAgent::Get(render_frame());
  ASSERT_TRUE(agent);
  render_frame()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      optimization_guide::mojom::PageTextService::Name_,
      base::BindRepeating(
          [&](optimization_guide::PageTextAgent* agent,
              mojo::ScopedInterfaceEndpointHandle handle) {
            agent->Bind(mojo::PendingAssociatedReceiver<
                        optimization_guide::mojom::PageTextService>(
                std::move(handle)));
          },
          agent));

  mojo::PendingRemote<optimization_guide::mojom::PageTextConsumer>
      consumer_remote;
  TestOptGuideConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = optimization_guide::mojom::PageTextDumpRequest::New();
  request->max_size = 123;
  request->event = optimization_guide::mojom::TextDumpEvent::kFirstLayout;

  mojo::AssociatedRemote<optimization_guide::mojom::PageTextService>
      text_service;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&text_service);
  text_service->RequestPageTextDump(std::move(request),
                                    std::move(consumer_remote));
  base::RunLoop().RunUntilIdle();

  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(u"foo", consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
}

TEST_F(ChromeRenderFrameObserverTest, BenchmarkingUndefinedByDefault) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome === 'undefined' || typeof chrome.benchmarking === "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome === 'undefined' || typeof chrome.Interval === "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverWithBenchmarkingTest, BenchmarkingEnabled) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.benchmarking !== "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverWithBenchmarkingTest, BenchmarkingMethods) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // Check isSingleProcess
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.isSingleProcess === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // Check getRendererPid
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.getRendererPid === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // Check getRendererMainTid
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.getRendererMainTid === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // Verify they return correct types/values
  int pid = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"chrome.benchmarking.getRendererPid()", &pid));
  EXPECT_EQ(static_cast<int>(base::GetCurrentProcId()), pid);

  int tid = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"chrome.benchmarking.getRendererMainTid()", &tid));
  EXPECT_EQ(
      base::PlatformThread::CurrentId().truncate_to_int32_for_display_only(),
      tid);

  int is_single_process = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"chrome.benchmarking.isSingleProcess() ? 1 : 0", &is_single_process));
  int expected_is_single_process =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)
          ? 1
          : 0;
  EXPECT_EQ(expected_is_single_process, is_single_process);

  // Check getMarkFunctions
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.getMarkFunctions === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"var marks = chrome.benchmarking.getMarkFunctions();"
      u"typeof marks === 'object' && "
      u"typeof marks.start === 'object' && "
      u"typeof marks.stop === 'object' && "
      u"typeof marks.start.function === 'function' && "
      u"typeof marks.stop.function === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverWithBenchmarkingTest, BenchmarkingInterval) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // Check hiResTime exists
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.hiResTime === 'function' ? 1 : 0", &result));
  EXPECT_EQ(1, result);

  // Check chrome.Interval exists
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.Interval === 'function' ? 1 : 0", &result));
  EXPECT_EQ(1, result);

  // Test chrome.Interval functionality
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"var interval = new chrome.Interval();"
      u"interval.start();"
      u"interval.stop();"
      u"var delta = interval.microseconds();"
      u"delta >= 0 ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // Test that hiResTime returns increasing values
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"var t1 = chrome.benchmarking.hiResTime();"
      u"var t2 = chrome.benchmarking.hiResTime();"
      u"t2 >= t1 ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverWithBenchmarkingTest,
       NetBenchmarkingUndefinedWithoutSwitch) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // chrome.benchmarking should be defined.
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.benchmarking !== "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // but chrome.benchmarking.clearCache should not be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.clearCache === 'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

class ChromeRenderFrameObserverWithNetBenchmarkingTest
    : public ChromeRenderFrameObserverWithBenchmarkingTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNetBenchmarking);
    ChromeRenderFrameObserverWithBenchmarkingTest::SetUp();
  }
};

TEST_F(ChromeRenderFrameObserverWithNetBenchmarkingTest,
       NetBenchmarkingEnabledWithSwitch) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // chrome.benchmarking should be defined.
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.benchmarking !== "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // and chrome.benchmarking.clearCache should be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.clearCache === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // and chrome.benchmarking.clearHostResolverCache should be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.clearHostResolverCache === 'function' ? 1 : "
      u"0",
      &result));
  EXPECT_EQ(1, result);

  // and chrome.benchmarking.clearPredictorCache should be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.clearPredictorCache === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // and chrome.benchmarking.closeConnections should be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.closeConnections === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverWithNetBenchmarkingTest,
       NetBenchmarkingMethodsCanBeCalled) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"try {"
      u"  chrome.benchmarking.clearCache();"
      u"  chrome.benchmarking.clearHostResolverCache();"
      u"  chrome.benchmarking.clearPredictorCache();"
      u"  chrome.benchmarking.closeConnections();"
      u"  1;"
      u"} catch(e) {"
      u"  0;"
      u"}",
      &result));
  EXPECT_EQ(1, result);
}

class ChromeRenderFrameObserverWithOnlyNetBenchmarkingTest
    : public ChromeRenderFrameObserverTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNetBenchmarking);
    ChromeRenderFrameObserverTest::SetUp();
  }
};

TEST_F(ChromeRenderFrameObserverWithOnlyNetBenchmarkingTest,
       NetBenchmarkingEnabledWithSwitch) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // chrome.benchmarking should be defined.
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.benchmarking !== "
      u"'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // and chrome.benchmarking.clearCache should be defined.
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.clearCache === 'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // BUT benchmarking methods should NOT be defined (e.g., hiResTime).
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome.benchmarking.hiResTime === 'undefined' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverTest, LoadTimesAndCsiDefinedUnconditionally) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // Check chrome.loadTimes exists and is a function
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.loadTimes === "
      u"'function' ? 1 : 0",
      &result));
  EXPECT_EQ(1, result);

  // Check chrome.csi exists and is a function
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"typeof chrome !== 'undefined' && typeof chrome.csi === 'function' ? 1 "
      u": 0",
      &result));
  EXPECT_EQ(1, result);
}

TEST_F(ChromeRenderFrameObserverTest, LoadTimesAndCsiValues) {
  LoadHTML("<html><body></body></html>");

  int result = -1;
  // Test chrome.loadTimes() return value structure
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"(function() {"
      u"  const lt = chrome.loadTimes();"
      u"  if (typeof lt !== 'object' || lt === null) return 0;"
      u"  const expectedKeys = ["
      u"    'requestTime', 'startLoadTime', 'commitLoadTime', "
      u"    'finishDocumentLoadTime', 'finishLoadTime', 'firstPaintTime', "
      u"    'firstPaintAfterLoadTime', 'navigationType', 'wasFetchedViaSpdy', "
      u"    'wasNpnNegotiated', 'npnNegotiatedProtocol', "
      u"    'wasAlternateProtocolAvailable', 'connectionInfo'"
      u"  ];"
      u"  for (const key of expectedKeys) {"
      u"    if (!(key in lt)) return 2;"
      u"  }"
      u"  if (typeof lt.requestTime !== 'number') return 3;"
      u"  if (typeof lt.startLoadTime !== 'number') return 4;"
      u"  if (typeof lt.commitLoadTime !== 'number') return 5;"
      u"  if (typeof lt.finishDocumentLoadTime !== 'number') return 6;"
      u"  if (typeof lt.finishLoadTime !== 'number') return 7;"
      u"  if (typeof lt.firstPaintTime !== 'number') return 8;"
      u"  if (typeof lt.firstPaintAfterLoadTime !== 'number') return 9;"
      u"  if (typeof lt.navigationType !== 'string') return 10;"
      u"  if (typeof lt.wasFetchedViaSpdy !== 'boolean') return 11;"
      u"  if (typeof lt.wasNpnNegotiated !== 'boolean') return 12;"
      u"  if (typeof lt.npnNegotiatedProtocol !== 'string') return 13;"
      u"  if (typeof lt.wasAlternateProtocolAvailable !== 'boolean') return 14;"
      u"  if (typeof lt.connectionInfo !== 'string') return 15;"
      u"  return 1;"
      u"})()",
      &result));
  EXPECT_EQ(1, result);

  // Test chrome.csi() return value structure
  result = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"(function() {"
      u"  const csi = chrome.csi();"
      u"  if (typeof csi !== 'object' || csi === null) return 0;"
      u"  const expectedKeys = ['startE', 'onloadT', 'pageT', 'tran'];"
      u"  for (const key of expectedKeys) {"
      u"    if (!(key in csi)) return 2;"
      u"  }"
      u"  if (typeof csi.startE !== 'number') return 3;"
      u"  if (typeof csi.onloadT !== 'number') return 4;"
      u"  if (typeof csi.pageT !== 'number') return 5;"
      u"  if (typeof csi.tran !== 'number') return 6;"
      u"  return 1;"
      u"})()",
      &result));
  EXPECT_EQ(1, result);
}
