// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

class TestDelegateTwoOrigins : public permissions::PermissionPrompt::Delegate {
 private:
  const GURL embedding_origin_;

 public:
  explicit TestDelegateTwoOrigins(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const std::vector<permissions::RequestType> request_types)
      : embedding_origin_(embedding_origin) {
    requests_ = base::ToVector(
        request_types,
        [&](auto& request_type)
            -> std::unique_ptr<permissions::PermissionRequest> {
          return std::make_unique<permissions::MockPermissionRequest>(
              requesting_origin, request_type);
        });
    raw_requests_ = base::ToVector(
        requests_,
        [](const auto& request)
            -> raw_ptr<permissions::PermissionRequest, VectorExperimental> {
          return request.get();
        });
  }

  const std::vector<
      raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
  Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override { return embedding_origin_; }

  void Accept() override {}
  void AcceptThisTime() override {}
  void Deny() override {}
  void Dismiss() override {}
  void Ignore() override {}
  void FinalizeCurrentRequests() override {}
  void OpenHelpCenterLink(const ui::Event& event) override {}
  void PreIgnoreQuietPrompt() override {}
  void SetManageClicked() override {}
  void SetLearnMoreClicked() override {}
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}

  bool WasCurrentRequestAlreadyDisplayed() override { return false; }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  std::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return std::nullopt;
  }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}
  bool RecreateView() override { return false; }

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  content::WebContents* GetAssociatedWebContents() override { return nullptr; }

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests_;
  base::WeakPtrFactory<TestDelegateTwoOrigins> weak_factory_{this};
};
}  // namespace

class PermissionPromptBubbleTwoOriginsViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    CreateBrowser();
  }

  Browser* browser() { return browser_.get(); }

  std::unique_ptr<PermissionPromptBubbleBaseView> CreateBubble(
      TestDelegateTwoOrigins* delegate) {
    return std::make_unique<PermissionPromptBubbleTwoOriginsView>(
        browser(), delegate->GetWeakPtr(), base::TimeTicks::Now(),
        PermissionPromptStyle::kBubbleOnly);
  }

 private:
  void CreateBrowser() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        FaviconServiceFactory::GetInstance(),
        FaviconServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_.get(), /*user_gesture=*/true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
};

TEST_F(PermissionPromptBubbleTwoOriginsViewTest,
       TitleMentionsRequestingOriginAndPermission) {
  TestDelegateTwoOrigins delegate(GURL("https://www.test.requesting.com"),
                                  GURL("https://www.test.embedding.com"),
                                  {permissions::RequestType::kStorageAccess});
  auto bubble = CreateBubble(&delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "info they've saved about you",
                      title);
  // The scheme is not included. Only the origin should be visible.
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "requesting.com", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "https://", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "www", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "test.requesting", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "test.embedding", title);
}

TEST_F(PermissionPromptBubbleTwoOriginsViewTest, DiesIfPermissionNotAllowed) {
  TestDelegateTwoOrigins delegate(GURL("https://www.test.requesting.com"),
                                  GURL("https://www.test.embedding.com"),
                                  {permissions::RequestType::kCameraStream});
  EXPECT_DEATH_IF_SUPPORTED(CreateBubble(&delegate), "");
}

TEST_F(PermissionPromptBubbleTwoOriginsViewTest,
       DescriptionMentionsTwoOriginsAndPermission) {
  TestDelegateTwoOrigins delegate(GURL("https://www.test.requesting.com"),
                                  GURL("https://www.test.embedding.com"),
                                  {permissions::RequestType::kStorageAccess});
  auto bubble = CreateBubble(&delegate);

  auto* label_description = static_cast<views::Label*>(
      bubble->GetViewByID(permissions::PermissionPromptViewID::
                              VIEW_ID_PERMISSION_PROMPT_EXTRA_TEXT));
  EXPECT_TRUE(label_description);

  // The scheme is not included. Only the origin should be visible.
  const auto description = base::UTF16ToUTF8(label_description->GetText());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "requesting.com", description);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "embedding.com", description);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "https://", description);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "www", description);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "test.requesting",
                      description);
}

TEST_F(PermissionPromptBubbleTwoOriginsViewTest, LinkIsPresent) {
  TestDelegateTwoOrigins delegate(GURL("https://www.test.requesting.com"),
                                  GURL("https://www.test.embedding.com"),
                                  {permissions::RequestType::kStorageAccess});
  auto bubble = CreateBubble(&delegate);

  auto* label_with_link = static_cast<views::StyledLabel*>(bubble->GetViewByID(
      permissions::PermissionPromptViewID::VIEW_ID_PERMISSION_PROMPT_LINK));
  EXPECT_TRUE(label_with_link);
  const auto link = base::UTF16ToUTF8(label_with_link->GetText());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "Learn more", link);
}

// TODO(b/276716358): Add behavior tests to ensure the prompt works and updates
// the content setting accordingly when accepted/declined.
