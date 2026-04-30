// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_DELEGATE_H_

#include <memory>
#include <vector>

#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace permissions {

class MockPermissionPromptDelegate : public PermissionPrompt::Delegate {
 public:
  MockPermissionPromptDelegate();
  ~MockPermissionPromptDelegate() override;

  MOCK_METHOD(const std::vector<std::unique_ptr<PermissionRequest>>&,
              Requests,
              (),
              (override));
  MOCK_METHOD(GURL, GetRequestingOrigin, (), (const, override));
  MOCK_METHOD(GURL, GetEmbeddingOrigin, (), (const, override));
  MOCK_METHOD(void, Accept, (const PromptOptions&), (override));
  MOCK_METHOD(void, AcceptThisTime, (const PromptOptions&), (override));
  MOCK_METHOD(void, Deny, (const PromptOptions&), (override));
  MOCK_METHOD(void, Dismiss, (const PromptOptions&), (override));
  MOCK_METHOD(void, Ignore, (const PromptOptions&), (override));
  MOCK_METHOD(void, SwitchToLoudPrompt, (), (override));
  MOCK_METHOD(GeolocationAccuracy,
              GetInitialGeolocationAccuracySelection,
              (),
              (const, override));
  MOCK_METHOD(void, FinalizeCurrentRequests, (), (override));
  MOCK_METHOD(void, OpenHelpCenterLink, (const ui::Event&), (override));
  MOCK_METHOD(void, PreIgnoreQuietPrompt, (), (override));
  MOCK_METHOD(std::optional<PermissionUiSelector::QuietUiReason>,
              ReasonForUsingQuietUi,
              (),
              (const, override));
  MOCK_METHOD(bool, ShouldCurrentRequestUseQuietUI, (), (const, override));
  MOCK_METHOD(bool,
              ShouldDropCurrentRequestIfCannotShowQuietly,
              (),
              (const, override));
  MOCK_METHOD(bool, ShouldShowLocationPrecisionSelector, (), (const, override));
  MOCK_METHOD(bool, WasCurrentRequestAlreadyDisplayed, (), (override));
  MOCK_METHOD(void, SetDismissOnTabClose, (), (override));
  MOCK_METHOD(void, SetPromptShown, (), (override));
  MOCK_METHOD(void, SetDecisionTime, (), (override));
  MOCK_METHOD(void, SetManageClicked, (), (override));
  MOCK_METHOD(void, SetLearnMoreClicked, (), (override));
  MOCK_METHOD(void,
              SetHatsShownCallback,
              (base::OnceCallback<void()>),
              (override));
  MOCK_METHOD(content::WebContents*, GetAssociatedWebContents, (), (override));
  MOCK_METHOD(base::WeakPtr<Delegate>, GetWeakPtr, (), (override));
  MOCK_METHOD(bool, RecreateView, (), (override));
  MOCK_METHOD(const PermissionPrompt*, GetCurrentPrompt, (), (const, override));
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_DELEGATE_H_
