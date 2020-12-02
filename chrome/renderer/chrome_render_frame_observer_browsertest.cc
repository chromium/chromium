// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <tuple>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame_widget.h"
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
      bool page_level_translation_critiera_met) override {
    called_new_page_ = true;
    page_level_translation_critiera_met_ = page_level_translation_critiera_met;
  }

  bool called_new_page_ = false;
  bool page_level_translation_critiera_met_ = false;

 private:
  mojo::ReceiverSet<translate::mojom::ContentTranslateDriver> receivers_;
};

}  // namespace

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

class ChromeRenderFrameObserverTest : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    view_->GetMainRenderFrame()
        ->GetBrowserInterfaceBroker()
        ->SetBinderForTesting(
            translate::mojom::ContentTranslateDriver::Name_,
            base::BindRepeating(&FakeContentTranslateDriver::BindHandle,
                                base::Unretained(&fake_translate_driver_)));
  }

  void TearDown() override {
    view_->GetMainRenderFrame()
        ->GetBrowserInterfaceBroker()
        ->SetBinderForTesting(translate::mojom::ContentTranslateDriver::Name_,
                              {});

    ChromeRenderViewTest::TearDown();
  }

  FakeContentTranslateDriver fake_translate_driver_;
};

TEST_F(ChromeRenderFrameObserverTest, SkipCapturingSubFrames) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<!DOCTYPE html><body>"
      "This is a main document"
      "<iframe srcdoc=\"This a document in an iframe.\">"
      "</body>");
  view_->GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_critiera_met_)
      << "Page should be translatable.";
  // Should have 2 samples: one for preliminary capture, one for final capture.
  // If there are more, then subframes are being captured more than once.
  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 2);
}
