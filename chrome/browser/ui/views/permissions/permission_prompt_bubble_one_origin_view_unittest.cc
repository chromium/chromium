// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using PermissionPromptBubbleOneOriginViewTest = ChromeViewsTestBase;

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types) {
    base::ranges::transform(
        request_types, std::back_inserter(requests_), [&](auto& request_type) {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type);
        });
    base::ranges::transform(
        requests_, std::back_inserter(raw_requests_),
        &std::unique_ptr<permissions::PermissionRequest>::get);
  }

  const std::vector<permissions::PermissionRequest*>& Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  void Accept() override {}
  void AcceptThisTime() override {}
  void Deny() override {}
  void Dismiss() override {}
  void Ignore() override {}
  void PreIgnoreQuietPrompt() override {}
  void SetManageClicked() override {}
  void SetLearnMoreClicked() override {}
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}

  bool WasCurrentRequestAlreadyDisplayed() override { return false; }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  absl::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return absl::nullopt;
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
  std::vector<permissions::PermissionRequest*> raw_requests_;
  base::WeakPtrFactory<TestDelegate> weak_factory_{this};
};

std::unique_ptr<PermissionPromptBubbleOneOriginView> CreateBubble(
    TestDelegate* delegate) {
  return std::make_unique<PermissionPromptBubbleOneOriginView>(
      nullptr, delegate->GetWeakPtr(), base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);
}

}  // namespace

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleMentionsOriginAndPermissions) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kMicStream,
                         permissions::RequestType::kCameraStream});
  auto bubble = CreateBubble(&delegate);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "microphone",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "camera",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  // The scheme is not included.
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "test.origin",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleDoesNotMentionTooManyPermissions) {
  TestDelegate delegate(GURL(), {permissions::RequestType::kGeolocation,
                                 permissions::RequestType::kNotifications,
                                 permissions::RequestType::kMicStream,
                                 permissions::RequestType::kCameraStream});
  auto bubble = CreateBubble(&delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "location", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "notifications", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "microphone", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "camera", title);
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleFileSchemeMentionsThisFile) {
  TestDelegate delegate(GURL("file:///tmp/index.html"),
                        {permissions::RequestType::kMicStream});
  auto bubble = CreateBubble(&delegate);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring,
                      base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                          IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE)),
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleIncludesOnlyVisiblePermissions) {
  TestDelegate delegate(GURL(), {permissions::RequestType::kMicStream,
                                 permissions::RequestType::kCameraStream,
                                 permissions::RequestType::kCameraPanTiltZoom});
  auto bubble = CreateBubble(&delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "microphone", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "move your camera", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "use your camera", title);
}
