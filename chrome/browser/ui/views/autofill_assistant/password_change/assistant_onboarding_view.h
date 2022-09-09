// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

class AssistantOnboardingController;

// View that displays the onboarding/consent prompt for autofill assistant.
// It uses the text supplied by the AssistantOnboardingController
// to which it holds a weak pointer.
// As all other `DialogDelegateView` extensions, the `AssistantOnboardingView`
// is owned by the widget inside which it is placed after calling `Show()`.
// It must therefore created via `new` in the static factory function and must
// not be deleted manually. In addition, `Show()` should always be called after
// constructing it.
// The view and its controller notify each other when one is destroyed.
//
// IMPORTANT: If any additional text elements are added, then these must be
// included in `OnAccept` to ensure that the audit record is complete.
class AssistantOnboardingView : public views::DialogDelegateView,
                                public AssistantOnboardingPrompt {
 public:
  METADATA_HEADER(AssistantOnboardingView);

  // IDs that identify a view within the dialog that was used in browsertests.
  enum class DialogViewID : int {
    VIEW_ID_NONE = 0,
    HEADER_ICON,
    TITLE,
    DESCRIPTION,
    CONSENT_TEXT,
  };

  explicit AssistantOnboardingView(
      base::WeakPtr<AssistantOnboardingController> controller);
  ~AssistantOnboardingView() override;

  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;

  // AssistantOnboardingPrompt:
  void Show(content::WebContents* web_contents) override;
  void OnControllerGone() override;

  // Returns a weak pointer to itself.
  base::WeakPtr<AssistantOnboardingView> GetWeakPtr();

 private:
  // Sets up the parameters of `DialogDelegate`.
  void InitDelegate();

  // Creates the content of the dialog by adding the relevant views.
  void InitDialog();

  // Informs the controller that consent was accepted and passes the resource
  // ids of the strings on the accept button and the other text elements of
  // the dialog.
  void OnAccept();

  // The controller belonging to this view.
  base::WeakPtr<AssistantOnboardingController> controller_;

  // Factory for weak pointers to this view.
  base::WeakPtrFactory<AssistantOnboardingView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_
