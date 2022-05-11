// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// View that displays the onboarding/ consent prompt for autofill assistant.
// It uses the legal text supplied by the AssistantOnboardingController
// to which it holds a raw pointer.
// The AssistantOnboardingView is owned by the |views::View| that it is a child
// of. It therefore needs to be created via new/ a factory method and passes
// ownership of itself to the |AssistantDisplayDelegate| during its
// construction.
// The View and its controller notify each other when one is destroyed so
// that the other can invalidate the pointer it holds.
// TODO(crbug.com/1322387): Check whether the derive from BoxView or FlexView.
class AssistantOnboardingView : public views::View,
                                public AssistantOnboardingPrompt {
 public:
  METADATA_HEADER(AssistantOnboardingView);
  AssistantOnboardingView(AssistantOnboardingController* controller,
                          AssistantDisplayDelegate* display_delegate);
  ~AssistantOnboardingView() override;

  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;

  // AssistantOnboardingPrompt:
  void Show() override;
  void OnControllerGone() override;

  // Callbacks for the dialog buttons that inform the controller, null the
  // controller and close the view.
  void OnCancel();
  void OnAccept();

 private:
  // Closes the view by removing itself from the display. CHECKs that the raw
  // pointer to the controller is a nullptr.
  void Close();

  // The controller belonging to this view.
  raw_ptr<AssistantOnboardingController> controller_;

  // The display that owns this view.
  raw_ptr<AssistantDisplayDelegate> display_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_VIEW_H_
