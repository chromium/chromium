// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
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
      bool page_level_translation_critiera_met) override {
    called_new_page_ = true;
    page_level_translation_critiera_met_ = page_level_translation_critiera_met;
  }
  void GetLanguageDetectionModel(
      GetLanguageDetectionModelCallback callback) override {}

  bool called_new_page_ = false;
  bool page_level_translation_critiera_met_ = false;

 private:
  mojo::ReceiverSet<translate::mojom::ContentTranslateDriver> receivers_;
};

}  // namespace

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

class ChromeRenderFrameObserverTest : public ChromeRenderViewTest {
 public:
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

  content::RenderFrame* render_frame() { return view_->GetMainRenderFrame(); }

 protected:
  FakeContentTranslateDriver fake_translate_driver_;
};

// The "Translate.CapturePageText" histogram is used to check whether the
// |CapturePageText| method was run. It should have 2 samples: one for
// preliminary capture, one for final capture.

TEST_F(ChromeRenderFrameObserverTest, CapturePageTextCalled) {
  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 2);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_critiera_met_);
}

TEST_F(ChromeRenderFrameObserverTest, CapturePageTextNotCalledForSubframe) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<!DOCTYPE html><body>"
      "This is a main document"
      "<iframe srcdoc=\"This a document in an iframe.\">"
      "</body>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 2);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_critiera_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForUpcomingNavigation) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<html><head>"
      "<meta http-equiv=\"refresh\" content=\"1\"></head>"
      "<body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForViewSourceMode) {
  base::HistogramTester histogram_tester;
  render_frame()->GetWebFrame()->EnableViewSourceMode(true);

  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForUnreachableURL) {
  base::HistogramTester histogram_tester;

  render_frame()->LoadHTMLStringForTesting("<html><body>foo</body></html>",
                                           GURL("data:text/html,"), "UTF-8",
                                           GURL("http://unreachable.com"),
                                           /*replace_current_item=*/false);

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

TEST_F(ChromeRenderFrameObserverTest,
       CapturePageTextNotCalledForNoStatePrefetch) {
  base::HistogramTester histogram_tester;

  prerender::NoStatePrefetchHelper helper(render_frame(), "");

  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)

class ChromeRenderFrameObserverNoTranslateNorPhishingTest
    : public ChromeRenderFrameObserverTest {
 public:
  ChromeRenderFrameObserverNoTranslateNorPhishingTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableClientSidePhishingDetection);
    scoped_feature_list_.InitAndEnableFeature(translate::kTranslateSubFrames);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeRenderFrameObserverNoTranslateNorPhishingTest,
       CapturePageTextNotCalled) {
  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

class ChromeRenderFrameObserverNoTranslateYesPhishingTest
    : public ChromeRenderFrameObserverTest {
 public:
  ChromeRenderFrameObserverNoTranslateYesPhishingTest() {
    scoped_feature_list_.InitAndEnableFeature(translate::kTranslateSubFrames);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeRenderFrameObserverNoTranslateYesPhishingTest,
       CapturePageTextCalled) {
  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 2);

  // Translate should not be called since only the phishing logic ran.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

#else

class ChromeRenderFrameObserverNoTranslateTest
    : public ChromeRenderFrameObserverTest {
 public:
  ChromeRenderFrameObserverNoTranslateNorPhishingTest() {
    scoped_feature_list_.InitAndEnableFeature(translate::kTranslateSubFrames);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeRenderFrameObserverNoTranslateTest, CapturePageTextNotCalled) {
  base::HistogramTester histogram_tester;
  LoadHTML("<html><body>foo</body></html>");

  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_critiera_met_);
}

#endif
