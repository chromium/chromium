// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

namespace {

class MockAmbientDelegate : public TestPermissionBubbleViewDelegate {
 public:
  explicit MockAmbientDelegate(const GURL& origin,
                               const std::u16string& username = u"",
                               const std::u16string& provider_name = u"") {
    std::vector<ambient_signin::PasskeyOrPasswordCredential> credentials;
    if (!username.empty() || !provider_name.empty()) {
      credentials.push_back(
          {username, provider_name, ambient_signin::CredentialType::kPasskey});
    }
    std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
    requests.push_back(
        std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
            origin, origin, std::move(credentials),
            base::BindOnce(&MockAmbientDelegate::OnCredentialSelected,
                           base::Unretained(this)),
            base::DoNothing()));
    set_requests(std::move(requests));
  }

  void OnCredentialSelected(size_t index) {
    selected_credential_index_ = index;
  }

  std::optional<size_t> selected_credential_index() const {
    return selected_credential_index_;
  }

  MOCK_METHOD(void, Accept, (const PromptOptions&), (override));
  MOCK_METHOD(void, Dismiss, (const PromptOptions&), (override));

 private:
  std::optional<size_t> selected_credential_index_;
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
    SetViewportBounds(gfx::Rect(0, 0, 800, 600));
  }

  void SetViewportBounds(const gfx::Rect& bounds) {
    anchor_widget_->SetBounds(bounds);
    native_view_host_->SetBoundsRect(gfx::Rect(bounds.size()));
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
    bubble->GetWidget()->Show();
    bubble->GetDialogClientView()->ResetViewShownTimeStampForTesting();
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
  ASSERT_EQ(bubble->children().size(), 4u);

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

  // 4. Expand button
  EXPECT_TRUE(views::IsViewClass<views::ImageButton>(bubble->children()[3]));
}

TEST_F(AmbientLoginPermissionBubbleViewTest, EmptyUsernameAndProviderFallback) {
  MockAmbientDelegate delegate(GURL("https://example.com"));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  ASSERT_EQ(bubble->children().size(), 4u);

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
  bubble->GetDialogClientView()->ResetViewShownTimeStampForTesting();

  views::MdTextButton* sign_in_button =
      views::AsViewClass<views::MdTextButton>(bubble->children()[2]);
  ASSERT_NE(sign_in_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(delegate, Accept).Times(1);
  EXPECT_CALL(delegate, Dismiss).Times(0);
  views::test::ButtonTestApi(sign_in_button).NotifyClick(click_event);
  EXPECT_EQ(delegate.selected_credential_index(), 0u);
}

TEST_F(AmbientLoginPermissionBubbleViewTest, CloseButtonClickedDismisses) {
  MockAmbientDelegate delegate(GURL("https://example.com"), u"bob@example.com",
                               u"Google Password Manager");
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  EXPECT_CALL(delegate, Dismiss).Times(1);
  EXPECT_CALL(delegate, Accept).Times(0);
  bubble->ClosingPermission();
}

TEST_F(AmbientLoginPermissionBubbleViewTest, FederatedCredentialsDisplay) {
  std::vector<ambient_signin::FederatedCredential> fed_creds = {
      {u"idp.example.com", u"Carol", u"carol@example.com",
       GURL("https://idp.example.com")}};
  std::optional<size_t> selected_fed_index;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(
      std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          /*credentials=*/
          std::vector<ambient_signin::PasskeyOrPasswordCredential>{},
          /*credential_selected_callback=*/base::NullCallback(),
          std::move(fed_creds),
          base::BindOnce(
              [](std::optional<size_t>* out, size_t index) { *out = index; },
              &selected_fed_index),
          base::DoNothing()));

  MockAmbientDelegate delegate(GURL("https://example.com"));
  delegate.set_requests(std::move(requests));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);
  bubble->GetDialogClientView()->ResetViewShownTimeStampForTesting();

  views::View* text_container = bubble->children()[1].get();
  auto* username_label =
      views::AsViewClass<views::Label>(text_container->children()[0]);
  ASSERT_NE(username_label, nullptr);
  EXPECT_EQ(username_label->GetText(), u"Carol");

  auto* provider_label =
      views::AsViewClass<views::Label>(text_container->children()[1]);
  ASSERT_NE(provider_label, nullptr);
  EXPECT_EQ(provider_label->GetText(), u"idp.example.com");

  views::MdTextButton* sign_in_button =
      views::AsViewClass<views::MdTextButton>(bubble->children()[2]);
  ASSERT_NE(sign_in_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_CALL(delegate, Accept).Times(1);
  views::test::ButtonTestApi(sign_in_button).NotifyClick(click_event);
  EXPECT_EQ(selected_fed_index, 0u);
}

TEST_F(AmbientLoginPermissionBubbleViewTest,
       ExpandButtonShowsAllCredentialsAndRemovesSignInAndExpandButtons) {
  std::vector<ambient_signin::PasskeyOrPasswordCredential> creds = {
      {u"alice@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"Alice Smith"},
      {u"same@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"same@example.com"},
      {u"bob@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPassword}};
  std::vector<ambient_signin::FederatedCredential> fed_creds = {
      {u"idp.example.com", u"Carol", u"carol@example.com",
       GURL("https://idp.example.com")}};

  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(
      std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          std::move(creds), base::DoNothing(), std::move(fed_creds),
          base::DoNothing(), base::DoNothing()));

  MockAmbientDelegate delegate(GURL("https://example.com"));
  delegate.set_requests(std::move(requests));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  const int initial_height =
      bubble->GetWidget()->GetWindowBoundsInScreen().height();

  ASSERT_EQ(bubble->children().size(), 4u);
  views::ImageButton* expand_button =
      views::AsViewClass<views::ImageButton>(bubble->children()[3]);
  ASSERT_NE(expand_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(expand_button).NotifyClick(click_event);

  // Title bar is unchanged.
  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));
  views::Label* title_label =
      views::AsViewClass<views::Label>(bubble->GetBubbleFrameView()->title());
  ASSERT_NE(title_label, nullptr);
  EXPECT_EQ(title_label->GetText(),
            l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));

  // Sign In and expand buttons are removed; a ScrollView containing 4
  // credential rows is present.
  ASSERT_EQ(bubble->children().size(), 1u);
  views::ScrollView* scroll_view =
      views::AsViewClass<views::ScrollView>(bubble->children()[0]);
  ASSERT_NE(scroll_view, nullptr);
  views::View* list_view = scroll_view->contents();
  ASSERT_NE(list_view, nullptr);
  ASSERT_EQ(list_view->children().size(), 4u);

  // Row 0: Passkey with distinct display_name -> 3 text lines.
  HoverButton* row0 = views::AsViewClass<HoverButton>(list_view->children()[0]);
  ASSERT_NE(row0, nullptr);
  EXPECT_EQ(bubble->GetFocusManager()->GetFocusedView(), row0);
  views::View* row0_labels = row0->title()->parent();
  ASSERT_EQ(row0_labels->children().size(), 3u);
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row0_labels->children()[0])->GetText(),
      u"Alice Smith");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row0_labels->children()[1])->GetText(),
      u"alice@example.com");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row0_labels->children()[2])->GetText(),
      u"Google Password Manager");

  // Row 1: Passkey with display_name equal to username -> 2 text lines.
  HoverButton* row1 = views::AsViewClass<HoverButton>(list_view->children()[1]);
  ASSERT_NE(row1, nullptr);
  views::View* row1_labels = row1->title()->parent();
  ASSERT_EQ(row1_labels->children().size(), 2u);
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row1_labels->children()[0])->GetText(),
      u"same@example.com");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row1_labels->children()[1])->GetText(),
      u"Google Password Manager");

  // Row 2: Password -> 2 text lines.
  HoverButton* row2 = views::AsViewClass<HoverButton>(list_view->children()[2]);
  ASSERT_NE(row2, nullptr);
  views::View* row2_labels = row2->title()->parent();
  ASSERT_EQ(row2_labels->children().size(), 2u);
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row2_labels->children()[0])->GetText(),
      u"bob@example.com");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row2_labels->children()[1])->GetText(),
      u"Google Password Manager");

  // Row 3: Federated credential -> 3 text lines.
  HoverButton* row3 = views::AsViewClass<HoverButton>(list_view->children()[3]);
  ASSERT_NE(row3, nullptr);
  views::View* row3_labels = row3->title()->parent();
  ASSERT_EQ(row3_labels->children().size(), 3u);
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row3_labels->children()[0])->GetText(),
      u"Carol");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row3_labels->children()[1])->GetText(),
      u"carol@example.com");
  EXPECT_EQ(
      views::AsViewClass<views::Label>(row3_labels->children()[2])->GetText(),
      u"idp.example.com");

  // Height of the expanded bubble is larger than the collapsed bubble.
  EXPECT_GT(bubble->GetWidget()->GetWindowBoundsInScreen().height(),
            initial_height);
}

TEST_F(AmbientLoginPermissionBubbleViewTest,
       ExpandButtonClipsHeightToViewportWithBottomMargin) {
  SetViewportBounds(gfx::Rect(0, 0, 800, 220));

  std::vector<ambient_signin::PasskeyOrPasswordCredential> creds = {
      {u"user1@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"User 1"},
      {u"user2@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"User 2"},
      {u"user3@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPassword},
      {u"user4@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPassword}};
  std::vector<ambient_signin::FederatedCredential> fed_creds = {
      {u"idp.example.com", u"User 5", u"user5@example.com",
       GURL("https://idp.example.com")}};

  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(
      std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          std::move(creds), base::DoNothing(), std::move(fed_creds),
          base::DoNothing(), base::DoNothing()));

  MockAmbientDelegate delegate(GURL("https://example.com"));
  delegate.set_requests(std::move(requests));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);

  views::ImageButton* expand_button =
      views::AsViewClass<views::ImageButton>(bubble->children()[3]);
  ASSERT_NE(expand_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(expand_button).NotifyClick(click_event);

  ASSERT_EQ(bubble->children().size(), 1u);
  views::ScrollView* scroll_view =
      views::AsViewClass<views::ScrollView>(bubble->children()[0]);
  ASSERT_NE(scroll_view, nullptr);
  views::View* list_view = scroll_view->contents();
  ASSERT_NE(list_view, nullptr);
  ASSERT_EQ(list_view->children().size(), 5u);

  EXPECT_LT(scroll_view->GetPreferredSize().height(),
            list_view->GetPreferredSize().height());
  EXPECT_EQ(bubble->GetWidget()->GetWindowBoundsInScreen().bottom(),
            web_contents()->GetContainerBounds().bottom() - 48);
}

TEST_F(AmbientLoginPermissionBubbleViewTest,
       ClickingExpandedCredentialRowSelectsCredentialAndAccepts) {
  std::vector<ambient_signin::PasskeyOrPasswordCredential> creds = {
      {u"alice@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"Alice Smith"},
      {u"bob@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPassword}};
  std::vector<ambient_signin::FederatedCredential> fed_creds = {
      {u"idp.example.com", u"Carol", u"carol@example.com",
       GURL("https://idp.example.com")}};

  std::optional<size_t> selected_cred_index;
  std::optional<size_t> selected_fed_index;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(
      std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          std::move(creds),
          base::BindOnce(
              [](std::optional<size_t>* out, size_t index) { *out = index; },
              &selected_cred_index),
          std::move(fed_creds),
          base::BindOnce(
              [](std::optional<size_t>* out, size_t index) { *out = index; },
              &selected_fed_index),
          base::DoNothing()));

  MockAmbientDelegate delegate(GURL("https://example.com"));
  delegate.set_requests(std::move(requests));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);
  bubble->GetDialogClientView()->ResetViewShownTimeStampForTesting();

  views::ImageButton* expand_button =
      views::AsViewClass<views::ImageButton>(bubble->children()[3]);
  ASSERT_NE(expand_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(expand_button).NotifyClick(click_event);

  ASSERT_EQ(bubble->children().size(), 1u);
  views::ScrollView* scroll_view =
      views::AsViewClass<views::ScrollView>(bubble->children()[0]);
  ASSERT_NE(scroll_view, nullptr);
  views::View* list_view = scroll_view->contents();
  ASSERT_NE(list_view, nullptr);
  ASSERT_EQ(list_view->children().size(), 3u);
  HoverButton* second_cred_row =
      views::AsViewClass<HoverButton>(list_view->children()[1]);
  ASSERT_NE(second_cred_row, nullptr);

  EXPECT_CALL(delegate, Accept).Times(1);
  views::test::ButtonTestApi(second_cred_row).NotifyClick(click_event);
  EXPECT_EQ(selected_cred_index, 1u);
  EXPECT_FALSE(selected_fed_index.has_value());
}

TEST_F(AmbientLoginPermissionBubbleViewTest,
       ClickingExpandedFederatedCredentialRowSelectsFederatedAndAccepts) {
  std::vector<ambient_signin::PasskeyOrPasswordCredential> creds = {
      {u"alice@example.com", u"Google Password Manager",
       ambient_signin::CredentialType::kPasskey, u"Alice Smith"}};
  std::vector<ambient_signin::FederatedCredential> fed_creds = {
      {u"idp1.example.com", u"Carol", u"carol@example.com",
       GURL("https://idp1.example.com")},
      {u"idp2.example.com", u"Dave", u"dave@example.com",
       GURL("https://idp2.example.com")}};

  std::optional<size_t> selected_cred_index;
  std::optional<size_t> selected_fed_index;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(
      std::make_unique<ambient_signin::AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          std::move(creds),
          base::BindOnce(
              [](std::optional<size_t>* out, size_t index) { *out = index; },
              &selected_cred_index),
          std::move(fed_creds),
          base::BindOnce(
              [](std::optional<size_t>* out, size_t index) { *out = index; },
              &selected_fed_index),
          base::DoNothing()));

  MockAmbientDelegate delegate(GURL("https://example.com"));
  delegate.set_requests(std::move(requests));
  AmbientLoginPermissionBubbleView* bubble = CreateBubble(&delegate);
  bubble->GetDialogClientView()->ResetViewShownTimeStampForTesting();

  views::ImageButton* expand_button =
      views::AsViewClass<views::ImageButton>(bubble->children()[3]);
  ASSERT_NE(expand_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(expand_button).NotifyClick(click_event);

  ASSERT_EQ(bubble->children().size(), 1u);
  views::ScrollView* scroll_view =
      views::AsViewClass<views::ScrollView>(bubble->children()[0]);
  ASSERT_NE(scroll_view, nullptr);
  views::View* list_view = scroll_view->contents();
  ASSERT_NE(list_view, nullptr);
  ASSERT_EQ(list_view->children().size(), 3u);
  HoverButton* second_fed_row =
      views::AsViewClass<HoverButton>(list_view->children()[2]);
  ASSERT_NE(second_fed_row, nullptr);

  EXPECT_CALL(delegate, Accept).Times(1);
  views::test::ButtonTestApi(second_fed_row).NotifyClick(click_event);
  EXPECT_FALSE(selected_cred_index.has_value());
  EXPECT_EQ(selected_fed_index, 1u);
}

}  // namespace
