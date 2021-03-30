// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using PermissionPromptBubbleViewTest = ChromeViewsTestBase;

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestDelegate(const std::vector<ContentSettingsType> content_types) {
    std::transform(
        content_types.begin(), content_types.end(),
        std::back_inserter(requests_), [&](auto& content_type) {
          return std::make_unique<permissions::MockPermissionRequest>(
              permissions::PermissionUtil::GetPermissionString(content_type),
              content_type);
        });
    std::transform(requests_.begin(), requests_.end(),
                   std::back_inserter(raw_requests_),
                   [](auto& req) { return req.get(); });
  }

  TestDelegate(const GURL& origin, const std::vector<std::string> names) {
    std::transform(
        names.begin(), names.end(), std::back_inserter(requests_),
        [&](auto& name) {
          return std::make_unique<permissions::MockPermissionRequest>(
              name, permissions::RequestType::kGeolocation, origin);
        });
    std::transform(requests_.begin(), requests_.end(),
                   std::back_inserter(raw_requests_),
                   [](auto& req) { return req.get(); });
  }

  const std::vector<permissions::PermissionRequest*>& Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->GetOrigin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  void Accept() override {}
  void AcceptThisTime() override {}
  void Deny() override {}
  void Closing() override {}

  bool WasCurrentRequestAlreadyDisplayed() override { return false; }

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<permissions::PermissionRequest*> raw_requests_;
};

TEST_F(PermissionPromptBubbleViewTest, AccessibleTitleMentionsPermissions) {
  TestDelegate delegate(GURL("https://test.origin"), {"foo", "bar"});
  auto bubble = std::make_unique<PermissionPromptBubbleView>(
      nullptr, &delegate, base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "foo",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "bar",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleViewTest, AccessibleTitleMentionsOrigin) {
  TestDelegate delegate(GURL("https://test.origin"), {"foo", "bar"});
  auto bubble = std::make_unique<PermissionPromptBubbleView>(
      nullptr, &delegate, base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);

  // Note that the scheme is not usually included.
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "test.origin",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleViewTest,
       AccessibleTitleDoesNotMentionTooManyPermissions) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {"foo", "bar", "baz", "quxx"});
  auto bubble = std::make_unique<PermissionPromptBubbleView>(
      nullptr, &delegate, base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "foo", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "bar", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "baz", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "quxx", title);
}

TEST_F(PermissionPromptBubbleViewTest,
       AccessibleTitleFileSchemeMentionsThisFile) {
  TestDelegate delegate(GURL("file:///tmp/index.html"), {"foo", "bar"});
  auto bubble = std::make_unique<PermissionPromptBubbleView>(
      nullptr, &delegate, base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring,
                      base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                          IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE)),
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleViewTest,
       AccessibleTitleIncludesOnlyVisiblePermissions) {
  TestDelegate delegate({ContentSettingsType::MEDIASTREAM_MIC,
                         ContentSettingsType::MEDIASTREAM_CAMERA,
                         ContentSettingsType::CAMERA_PAN_TILT_ZOOM});
  auto bubble = std::make_unique<PermissionPromptBubbleView>(
      nullptr, &delegate, base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "AudioCapture", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "CameraPanTiltZoom", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "VideoCapture", title);
}

}  // namespace
