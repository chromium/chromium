// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_TEST_UTIL_H_
#define CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_TEST_UTIL_H_

#include <vector>

#include "components/permissions/permission_prompt.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
class PermissionRequest;
}

class TestPermissionBubbleViewDelegate
    : public permissions::PermissionPrompt::Delegate {
 public:
  TestPermissionBubbleViewDelegate();

  TestPermissionBubbleViewDelegate(const TestPermissionBubbleViewDelegate&) =
      delete;
  TestPermissionBubbleViewDelegate& operator=(
      const TestPermissionBubbleViewDelegate&) = delete;

  ~TestPermissionBubbleViewDelegate() override;

  const std::vector<
      raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
  Requests() override;

  GURL GetRequestingOrigin() const override;

  GURL GetEmbeddingOrigin() const override;

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

  std::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override;
  bool ShouldCurrentRequestUseQuietUI() const override;
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override;
  bool WasCurrentRequestAlreadyDisplayed() override;
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}
  bool RecreateView() override;
  content::WebContents* GetAssociatedWebContents() override;

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override;

  void set_requests(
      std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
          requests) {
    requests_ = requests;
  }

 private:
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests_;
  base::WeakPtrFactory<TestPermissionBubbleViewDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_TEST_UTIL_H_
