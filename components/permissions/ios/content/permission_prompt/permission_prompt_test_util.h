// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_TEST_UTIL_H_
#define COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_TEST_UTIL_H_

#include "components/permissions/ios/content/permission_prompt/permission_prompt_ios.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace permissions {

class MockPermissionPromptDelegate : public PermissionPrompt::Delegate {
 public:
  MockPermissionPromptDelegate();
  ~MockPermissionPromptDelegate() override;

  // PermissionPrompt::Delegate:
  const std::vector<std::unique_ptr<PermissionRequest>>& Requests() override;

  GURL GetRequestingOrigin() const override;

  GURL GetEmbeddingOrigin() const override;

  void Accept() override;
  void AcceptThisTime() override;
  void Deny() override;
  void Dismiss() override;
  void Ignore() override;

  GeolocationAccuracy GetInitialGeolocationAccuracySelection() const override;
  void SetPromptOptions(PromptOptions prompt_options) override;
  void FinalizeCurrentRequests() override;
  void OpenHelpCenterLink(const ui::Event& event) override;
  void PreIgnoreQuietPrompt() override;

  std::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const override;

  bool ShouldCurrentRequestUseQuietUI() const override;
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override;
  bool WasCurrentRequestAlreadyDisplayed() override;

  void SetDismissOnTabClose() override;
  void SetPromptShown() override;
  void SetDecisionTime() override;
  void SetManageClicked() override;
  void SetLearnMoreClicked() override;
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override;

  content::WebContents* GetAssociatedWebContents() override;

  base::WeakPtr<Delegate> GetWeakPtr() override;

  bool RecreateView() override;

  const PermissionPrompt* GetCurrentPrompt() const override;

  void AddRequest(std::unique_ptr<PermissionRequest> request);

  bool accept_called() const;
  bool accept_this_time_called() const;
  bool deny_called() const;

 private:
  std::vector<std::unique_ptr<PermissionRequest>> requests_;
  bool accept_called_ = false;
  bool accept_this_time_called_ = false;
  bool deny_called_ = false;
  base::WeakPtrFactory<MockPermissionPromptDelegate> weak_factory_{this};
};

class MockPermissionPromptIOS : public PermissionPromptIOS {
 public:
  MockPermissionPromptIOS(content::WebContents* web_contents,
                          Delegate* delegate);

  ~MockPermissionPromptIOS() override;

  // PermissionPrompt:
  PermissionPromptDisposition GetPromptDisposition() const override;

  // PermissionPromptIOS:
  NSString* GetPositiveButtonText(bool is_one_time) const override;

  NSString* GetNegativeButtonText(bool is_one_time) const override;

  NSString* GetPositiveEphemeralButtonText(bool is_one_time) const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_TEST_UTIL_H_
