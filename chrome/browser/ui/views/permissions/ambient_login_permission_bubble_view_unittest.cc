// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class MockAmbientDelegate : public TestPermissionBubbleViewDelegate {
 public:
  explicit MockAmbientDelegate(const GURL& origin,
                               const std::u16string& username = u"",
                               const std::u16string& provider_name = u"") {
    std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
    requests.push_back(
        std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
            origin, origin, base::DoNothing(), username, provider_name));
    set_requests(std::move(requests));
  }

  MOCK_METHOD(void, Accept, (const PromptOptions&), (override));
  MOCK_METHOD(void, Dismiss, (const PromptOptions&), (override));
};

class AmbientLoginPermissionBubbleViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL("https://example.com"));
    native_view_host_ = anchor_widget_->GetContentsView()->AddChildView(
        std::make_unique<views::NativeViewHost>());
    native_view_host_->Attach(web_contents_->GetNativeView());
  }

  void TearDown() override {
    if (bubble_tracker_.view()) {
      bubble_tracker_.view()->GetWidget()->CloseNow();
    }
    native_view_host_ = nullptr;
    anchor_widget_.reset();
    web_contents_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  AmbientLoginPermissionBubbleView* CreateBubble(
      MockAmbientDelegate* delegate) {
    auto* bubble = new AmbientLoginPermissionBubbleView(
        web_contents(), delegate->GetWeakPtr(),
        PermissionPromptStyle::kBubbleOnly);
    bubble->set_parent_window(anchor_widget_->GetNativeView());
    views::BubbleDialogDelegateView::CreateBubble(bubble);
    bubble_tracker_.SetView(bubble);
    return bubble;
  }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::NativeViewHost> native_view_host_ = nullptr;
  views::ViewTracker bubble_tracker_;
};

TEST_F(AmbientLoginPermissionBubbleViewTest,
       CreatesHorizontalLayoutWithUsernameAndProvider) {
  MockAmbientDelegate delegate(GURL("https://example.com"),
                               u"alice@example.com",
                               u"Google Password Manager");
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  ASSERT_NE(bubble->GetWidget(), nullptr);
  EXPECT_TRUE(bubble->ShouldShowCloseButton());
  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));
  auto* title_label =
      views::AsViewClass<views::Label>(bubble->GetBubbleFrameView()->title());
  ASSERT_NE(title_label, nullptr);
  EXPECT_EQ(title_label->GetText(),
            l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));
  EXPECT_EQ(title_label->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kHeading);
  EXPECT_EQ(bubble->children().size(), 3u);

  // 1. Icon view
  EXPECT_TRUE(views::IsViewClass<views::ImageView>(bubble->children()[0]));

  // 2. Text container view
  views::View* text_container = bubble->children()[1].get();
  EXPECT_EQ(text_container->children().size(), 2u);

  auto* username_label =
      views::AsViewClass<views::Label>(text_container->children()[0]);
  ASSERT_NE(username_label, nullptr);
  EXPECT_EQ(username_label->GetText(), u"alice@example.com");

  auto* provider_label =
      views::AsViewClass<views::Label>(text_container->children()[1]);
  ASSERT_NE(provider_label, nullptr);
  EXPECT_EQ(provider_label->GetText(), u"Google Password Manager");

  // 3. Sign In button
  auto* sign_in_button =
      views::AsViewClass<views::MdTextButton>(bubble->children()[2]);
  ASSERT_NE(sign_in_button, nullptr);
  EXPECT_EQ(
      sign_in_button->GetText(),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  EXPECT_EQ(sign_in_button->GetViewAccessibility().GetCachedDescription(),
            u"alice@example.com Google Password Manager");
}

TEST_F(AmbientLoginPermissionBubbleViewTest, EmptyUsernameAndProviderFallback) {
  MockAmbientDelegate delegate(GURL("https://example.com"));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  ASSERT_EQ(bubble->children().size(), 3u);

  views::View* text_container = bubble->children()[1].get();
  ASSERT_EQ(text_container->children().size(), 1u);

  auto* username_label =
      views::AsViewClass<views::Label>(text_container->children()[0]);
  ASSERT_NE(username_label, nullptr);
  const std::u16string expected_empty_login =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  EXPECT_EQ(username_label->GetText(), expected_empty_login);

  auto* sign_in_button =
      views::AsViewClass<views::MdTextButton>(bubble->children()[2]);
  ASSERT_NE(sign_in_button, nullptr);
  EXPECT_EQ(sign_in_button->GetViewAccessibility().GetCachedDescription(),
            expected_empty_login);
}

TEST_F(AmbientLoginPermissionBubbleViewTest, SignInButtonClickedAccepts) {
  MockAmbientDelegate delegate(GURL("https://example.com"), u"bob@example.com",
                               u"Google Password Manager");
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  EXPECT_CALL(delegate, Accept).Times(1);
  EXPECT_CALL(delegate, Dismiss).Times(0);
  bubble->RunButtonCallback(static_cast<int>(
      PermissionPromptBubbleBaseView::PermissionDialogButton::kAccept));
}

TEST_F(AmbientLoginPermissionBubbleViewTest, CloseButtonClickedDismisses) {
  MockAmbientDelegate delegate(GURL("https://example.com"), u"bob@example.com",
                               u"Google Password Manager");
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  EXPECT_CALL(delegate, Dismiss).Times(1);
  EXPECT_CALL(delegate, Accept).Times(0);
  bubble->ClosingPermission();
}

}  // namespace
