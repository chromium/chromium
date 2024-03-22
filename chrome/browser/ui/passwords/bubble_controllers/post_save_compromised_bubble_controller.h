// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "ui/gfx/range/range.h"

class PasswordsModelDelegate;

// This controller manages the bubble notifying the user about pending
// compromised credentials.
class PostSaveCompromisedBubbleController
    : public PasswordBubbleControllerBase {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BubbleType {
    // Last compromised password was updated. The user is presumed safe.
    kPasswordUpdatedSafeState = 0,
    // A compromised password was updated and there are more issues to fix.
    kPasswordUpdatedWithMoreToFix = 1,
    // kUnsafeState = 2, // was dropped
    kMaxValue = kPasswordUpdatedWithMoreToFix,
  };
  explicit PostSaveCompromisedBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~PostSaveCompromisedBubbleController() override;

  BubbleType type() const { return type_; }
  std::u16string GetBody();
  gfx::Range GetSettingLinkRange() const;
  std::u16string GetButtonText() const;
  int GetImageID(bool dark) const;

  // The user chose to check passwords.
  void OnAccepted();

  // The user chose to view passwords.
  void OnSettingsClicked();

 private:
  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  BubbleType type_;
  // Link to the settings range in the body text.
  gfx::Range link_range_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_POST_SAVE_COMPROMISED_BUBBLE_CONTROLLER_H_
