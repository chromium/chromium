// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"

class PasswordChangeRunController;

// Abstract interface for the view component of a password change script run.
class PasswordChangeRunDisplay {
 public:
  // A struct to define a prompt choice.
  struct PromptChoice {
    // The text displayed on the button.
    std::u16string text;
    // Whether the button is highlighted in blue or not.
    bool highlighted;
  };

  // Factory function to create a password change run. The
  // actual implementation is in the `password_change_run_view.cc` file.
  static base::WeakPtr<PasswordChangeRunDisplay> Create(
      base::WeakPtr<PasswordChangeRunController> controller,
      raw_ptr<AssistantDisplayDelegate> display_delegate);

  virtual ~PasswordChangeRunDisplay() = default;

  // Shows password change run UI.
  virtual void Show() = 0;

  // The below methods are used to set UI.
  // They all persist state to a model owned by the controller and call the
  // sibling view methods.
  virtual void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) = 0;
  virtual void SetTitle(const std::u16string& title) = 0;
  virtual void SetDescription(const std::u16string& progress_description) = 0;
  virtual void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) = 0;

  // Shows a base prompt, i.e. a set of buttons. Relies on the controller
  // calling `ClearPrompt` to close.
  virtual void ShowBasePrompt(const std::vector<PromptChoice>& choices) = 0;

  // Shows a generated password prompt for the password passed as a parameter.
  // Offers two buttons, one to accept the generated password and one to
  // choose manually. Relies on the controller calling `ClearPrompt` to close.
  virtual void ShowUseGeneratedPasswordPrompt(
      const std::u16string& title,
      const std::u16string& generated_password,
      const std::u16string& description,
      const PromptChoice& manual_password_choice,
      const PromptChoice& generated_password_choice) = 0;

  // Clears the area that contains the prompt body.
  virtual void ClearPrompt() = 0;

  // Notifies the view that the controller was destroyed so that the view
  // can close itself.
  virtual void OnControllerGone() = 0;

 protected:
  PasswordChangeRunDisplay() = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_
