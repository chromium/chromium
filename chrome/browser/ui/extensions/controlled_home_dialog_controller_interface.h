// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_INTERFACE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ui_base_types.h"

// The controller for the ControlledHomeDialog. This class is responsible
// for both providing the display information (ShowParams) as well as handling
// the result of the dialog.
class ControlledHomeDialogControllerInterface {
 public:
  enum CloseAction {
    CLOSE_LEARN_MORE,
    CLOSE_EXECUTE,
    CLOSE_DISMISS_USER_ACTION,
    CLOSE_DISMISS_DEACTIVATION,
  };

  virtual ~ControlledHomeDialogControllerInterface() = default;

  // Returns true if the bubble should (still) be shown. Since bubbles are
  // sometimes shown asynchronously, they may be invalid by the time they would
  // be displayed.
  virtual bool ShouldShow() = 0;

  // Gets the text for the bubble's heading (title).
  virtual std::u16string GetHeadingText() = 0;

  // Gets the text for the bubble's body.
  virtual std::u16string GetBodyText() = 0;

  // Gets the text for the main button on the bubble.
  virtual std::u16string GetActionButtonText() = 0;

  // Gets the text for a dismiss button on the bubble. If this returns an empty
  // string, no button will be added.
  virtual std::u16string GetDismissButtonText() = 0;

  // Returns the id of the action to point to, or the empty string if the
  // bubble should point to the center of the actions container.
  virtual std::string GetAnchorActionId() = 0;

  // Called when the bubble is shown.
  virtual void OnBubbleShown() = 0;

  // Called when the bubble is closed with the type of action the user took.
  virtual void OnBubbleClosed(CloseAction action) = 0;

  // Returns true if the bubble should add the policy indicator to the bubble.
  virtual bool IsPolicyIndicationNeeded() const = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_INTERFACE_H_
