// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"

class PasswordChangeRunController;

// Abstract interface for the view component of a password change script run.
class PasswordChangeRunDisplay {
 public:
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
  virtual void ShowBasePrompt(const std::vector<std::string>& options) = 0;
  virtual void ShowSuggestedPasswordPrompt(
      const std::u16string& suggested_password) = 0;

  // TODO(crbug.com/1322419): Configure prompts

  // Notifies the view that the controller was destroyed so that the view
  // can close itself.
  virtual void OnControllerGone() = 0;

 protected:
  PasswordChangeRunDisplay() = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_DISPLAY_H_
