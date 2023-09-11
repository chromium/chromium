// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_

#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/permission_prompt.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

// Base view that provide security-related functionality to permission prompts.
// This class will:
// * Compute an URL identity and provide it to subclasses
// * Elide the title as needed if it would be too long
// * Filter unintended button presses
// * Ensure no button is selected by default to prevent unintended button
// presses
class PermissionPromptBaseView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(PermissionPromptBaseView);
  PermissionPromptBaseView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate);

  // views::BubbleDialogDelegateView:
  // Overridden to elide the prompt title if needed
  void AddedToWidget() override;

  // Overridden to provide input protection on dialog default buttons.
  bool ShouldIgnoreButtonPressedEventHandling(
      View* button,
      const ui::Event& event) const override;

 protected:
  // Performs clickjacking checks and executes the button callback if the click
  // is valid. Subclasses need to make sure to set this as the callback for
  // custom buttons in order for this to work. This function will call
  // |RunButtonCallback| if the checks pass.
  void FilterUnintenedEventsAndRunCallbacks(int button_view_id,
                                            const ui::Event& event);

  // Called if a button press event has passes the input protections checks.
  // Needs to be implemented.
  virtual void RunButtonCallback(int button_view_id) = 0;

  const UrlIdentity& GetUrlIdentityObject() const { return url_identity_; }

  static UrlIdentity GetUrlIdentity(
      Browser* browser,
      permissions::PermissionPrompt::Delegate& delegate);

 private:
  const UrlIdentity url_identity_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BASE_VIEW_H_
