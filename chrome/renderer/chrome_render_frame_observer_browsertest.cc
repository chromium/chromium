// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/optimization_guide/content/renderer/page_text_agent.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
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
