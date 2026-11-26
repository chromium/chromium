// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/thin_webview/internal/thin_webview.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "cc/slim/layer.h"
#include "components/thin_webview/compositor_view.h"
#include "components/thin_webview/thin_webview_initializer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace thin_webview::android {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;

class FakeCompositorView : public CompositorView {
 public:
  void SetRootLayer(scoped_refptr<cc::slim::Layer> layer) override {}
};

class MockThinWebViewInitializer : public ThinWebViewInitializer {
 public:
  MOCK_METHOD(void, SetUpTheming, (content::WebContents*), (override));
  MOCK_METHOD(void,
              UpdateColorProviderSource,
              (content::WebContents*, ui::WindowAndroid*),
              (override));
  MOCK_METHOD(void,
              AttachTabHelpers,
              (content::WebContents*, bool, bool),
              (override));
  MOCK_METHOD(void,
              SetContextMenuPopulatorFactory,
              (content::WebContents*, const base::android::JavaRef<jobject>&),
              (override));
};

}  // namespace

class ThinWebViewTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    previous_initializer_ = ThinWebViewInitializer::GetInstance();
    ThinWebViewInitializer::SetInstance(&initializer_);
    thin_webview_window_ = ui::WindowAndroid::CreateForTesting();
  }

  void TearDown() override {
    thin_webview_window_.reset();
    ThinWebViewInitializer::SetInstance(previous_initializer_);
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<ThinWebView> CreateThinWebView() {
    return std::make_unique<ThinWebView>(
        base::android::AttachCurrentThread(),
        base::android::ScopedJavaLocalRef<jobject>(), &compositor_view_,
        thin_webview_window_->get());
  }

  std::unique_ptr<content::WebContents> CreateWebContentsInWindow(
      ui::WindowAndroid* window) {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    window->AddChild(contents->GetNativeView());
    return contents;
  }

  void AttachWebContents(ThinWebView* thin_webview,
                         content::WebContents* contents,
                         bool support_theming = false) {
    thin_webview->SetWebContents(contents, /*delegate=*/nullptr,
                                 /*enable_permission_requests=*/false,
                                 support_theming,
                                 /*enable_browser_autofill=*/false);
  }

  FakeCompositorView compositor_view_;
  NiceMock<MockThinWebViewInitializer> initializer_;
  raw_ptr<ThinWebViewInitializer> previous_initializer_ = nullptr;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      thin_webview_window_;
};

TEST_F(ThinWebViewTest, CapturesHostWindowBeforeReparenting) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window = ui::WindowAndroid::CreateForTesting();
  host_window->get()->AddChild(web_contents()->GetNativeView());
  ASSERT_EQ(web_contents()->GetTopLevelNativeWindow(), host_window->get());

  // ViewAndroid is reparented under ThinWebView's internal window, while
  // UpdateColorProviderSource receives the host Activity window captured first.
  std::unique_ptr<ThinWebView> thin_webview = CreateThinWebView();
  EXPECT_CALL(initializer_, SetUpTheming).Times(0);
  EXPECT_CALL(initializer_,
              UpdateColorProviderSource(web_contents(), host_window->get()));
  AttachWebContents(thin_webview.get(), web_contents());
  Mock::VerifyAndClearExpectations(&initializer_);
  EXPECT_EQ(web_contents()->GetTopLevelNativeWindow(),
            thin_webview_window_->get());

  // When support_theming is true, SetUpTheming runs before
  // UpdateColorProviderSource so WebContentsThemeClient is ready.
  std::unique_ptr<content::WebContents> themed_contents =
      CreateWebContentsInWindow(host_window->get());
  std::unique_ptr<ThinWebView> themed_thin_webview = CreateThinWebView();
  {
    InSequence seq;
    EXPECT_CALL(initializer_, SetUpTheming(themed_contents.get()));
    EXPECT_CALL(initializer_, UpdateColorProviderSource(themed_contents.get(),
                                                        host_window->get()));
  }
  AttachWebContents(themed_thin_webview.get(), themed_contents.get(),
                    /*support_theming=*/true);
  Mock::VerifyAndClearExpectations(&initializer_);
}

TEST_F(ThinWebViewTest, ReattachingSameWebContentsPreservesHostWindow) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window1 = ui::WindowAndroid::CreateForTesting();
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window2 = ui::WindowAndroid::CreateForTesting();
  host_window1->get()->AddChild(web_contents()->GetNativeView());

  std::unique_ptr<ThinWebView> thin_webview = CreateThinWebView();
  EXPECT_CALL(initializer_,
              UpdateColorProviderSource(web_contents(), host_window1->get()));
  AttachWebContents(thin_webview.get(), web_contents());
  Mock::VerifyAndClearExpectations(&initializer_);

  // Re-attaching the same WebContents while already parented under
  // ThinWebView's internal window must not overwrite the host window with
  // `thin_webview_window_`.
  EXPECT_CALL(initializer_, UpdateColorProviderSource).Times(0);
  AttachWebContents(thin_webview.get(), web_contents());
  Mock::VerifyAndClearExpectations(&initializer_);

  // Explicitly moving the WebContents to a new host window before re-attaching
  // updates the ColorProviderSource to the new host window.
  host_window2->get()->AddChild(web_contents()->GetNativeView());
  EXPECT_CALL(initializer_,
              UpdateColorProviderSource(web_contents(), host_window2->get()));
  AttachWebContents(thin_webview.get(), web_contents());
  Mock::VerifyAndClearExpectations(&initializer_);
}

TEST_F(ThinWebViewTest, SwappingWebContentsResetsPreviousOnlyWhenStillHosted) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window = ui::WindowAndroid::CreateForTesting();
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      other_window = ui::WindowAndroid::CreateForTesting();

  std::unique_ptr<content::WebContents> wc1 =
      CreateWebContentsInWindow(host_window->get());
  std::unique_ptr<content::WebContents> wc2 =
      CreateWebContentsInWindow(host_window->get());
  std::unique_ptr<content::WebContents> wc3 =
      CreateWebContentsInWindow(host_window->get());
  std::unique_ptr<content::WebContents> wc4 =
      CreateWebContentsInWindow(host_window->get());

  std::unique_ptr<ThinWebView> thin_webview = CreateThinWebView();
  AttachWebContents(thin_webview.get(), wc1.get());
  Mock::VerifyAndClearExpectations(&initializer_);

  // 1. Swapping while `wc1` is still under `thin_webview_window_` resets `wc1`
  // before attaching `wc2`.
  {
    InSequence seq;
    EXPECT_CALL(initializer_, UpdateColorProviderSource(wc1.get(), nullptr));
    EXPECT_CALL(initializer_,
                UpdateColorProviderSource(wc2.get(), host_window->get()));
  }
  AttachWebContents(thin_webview.get(), wc2.get());
  Mock::VerifyAndClearExpectations(&initializer_);

  // 2. Detaching `wc2` first (as ThinWebViewHost does via
  // setTopLevelNativeWindow(null)) still resets `wc2` on swap.
  wc2->GetNativeView()->RemoveFromParent();
  {
    InSequence seq;
    EXPECT_CALL(initializer_, UpdateColorProviderSource(wc2.get(), nullptr));
    EXPECT_CALL(initializer_,
                UpdateColorProviderSource(wc3.get(), host_window->get()));
  }
  AttachWebContents(thin_webview.get(), wc3.get());
  Mock::VerifyAndClearExpectations(&initializer_);

  // 3. Reparenting `wc3` to another window before swapping skips resetting
  // `wc3` so its new host is not clobbered.
  other_window->get()->AddChild(wc3->GetNativeView());
  EXPECT_CALL(initializer_, UpdateColorProviderSource(wc3.get(), _)).Times(0);
  EXPECT_CALL(initializer_,
              UpdateColorProviderSource(wc4.get(), host_window->get()));
  AttachWebContents(thin_webview.get(), wc4.get());
  Mock::VerifyAndClearExpectations(&initializer_);
}

TEST_F(ThinWebViewTest, DestructorResetsOnlyWhenStillHosted) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window = ui::WindowAndroid::CreateForTesting();
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      other_window = ui::WindowAndroid::CreateForTesting();

  auto setup_attached = [&]() {
    std::unique_ptr<content::WebContents> wc =
        CreateWebContentsInWindow(host_window->get());
    std::unique_ptr<ThinWebView> thin_webview = CreateThinWebView();
    AttachWebContents(thin_webview.get(), wc.get());
    Mock::VerifyAndClearExpectations(&initializer_);
    return std::pair(std::move(wc), std::move(thin_webview));
  };

  // 1. Still under `thin_webview_window_` -> reset to nullptr.
  {
    auto [wc, thin_webview] = setup_attached();
    EXPECT_CALL(initializer_, UpdateColorProviderSource(wc.get(), nullptr));
    thin_webview.reset();
    Mock::VerifyAndClearExpectations(&initializer_);
  }

  // 2. Detached to nullptr -> reset to nullptr.
  {
    auto [wc, thin_webview] = setup_attached();
    wc->GetNativeView()->RemoveFromParent();
    EXPECT_CALL(initializer_, UpdateColorProviderSource(wc.get(), nullptr));
    thin_webview.reset();
    Mock::VerifyAndClearExpectations(&initializer_);
  }

  // 3. Already reparented to another window -> skip reset.
  {
    auto [wc, thin_webview] = setup_attached();
    other_window->get()->AddChild(wc->GetNativeView());
    EXPECT_CALL(initializer_, UpdateColorProviderSource).Times(0);
    thin_webview.reset();
    Mock::VerifyAndClearExpectations(&initializer_);
  }

  // 4. WebContents destroyed before ThinWebView -> no-op.
  {
    auto [wc, thin_webview] = setup_attached();
    EXPECT_CALL(initializer_, UpdateColorProviderSource).Times(0);
    wc.reset();
    thin_webview.reset();
    Mock::VerifyAndClearExpectations(&initializer_);
  }
}

}  // namespace thin_webview::android
