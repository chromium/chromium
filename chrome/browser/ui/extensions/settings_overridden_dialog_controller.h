// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"

namespace gfx {
struct VectorIcon;
}

// The controller for the SettingsOverriddenDialog. This class is responsible
// for both providing the display information (ShowParams) as well as handling
// the result of the dialog (i.e., the user input).
class SettingsOverriddenDialogController {
 public:
  // A struct describing the contents to be displayed in the dialog.
  struct ShowParams {
    std::u16string dialog_title;
    std::u16string message;

    // The icon to display, if any. If non-null, the VectorIcon should have
    // all its colors fully specified; otherwise a placehold grey color will
    // be used.
    raw_ptr<const gfx::VectorIcon> icon = nullptr;
  };

  // The result (i.e., user input) from the dialog being shown.
  // Do not reorder this enum; it's used in histograms.
  enum class DialogResult {
    // The user wants to change their settings back to the previous value.
    kChangeSettingsBack = 0,
    // The user wants to keep the new settings, as configured by the extension.
    kKeepNewSettings = 1,
    // The dialog was dismissed without the user making a decision through the
    // close ('x') button, escape key, or similar.
    kDialogDismissed = 2,
    // The dialog was dismissed because it was destroyed, e.g. from the parent
    // window closing.
    kDialogClosedWithoutUserAction = 3,

    kMaxValue = kDialogClosedWithoutUserAction,
  };

  virtual ~SettingsOverriddenDialogController() = default;

  // Returns true if the dialog should be displayed. NOTE: This may only be
  // called synchronously from construction; it does not handle asynchronous
  // changes to the extension system.
  // For instance:
  // auto controller =
  //    std::make_unique<SettingsOverriddenDialogController>(...);
  // if (controller->ShouldShow())
  //   <show native dialog>
  virtual bool ShouldShow() = 0;

  // Returns the ShowParams for the dialog. This may only be called if
  // ShouldShow() returns true. Similar to above, this may only be called
  // synchronously.
  virtual ShowParams GetShowParams() = 0;

  // Notifies the controller that the dialog has been shown.
  virtual void OnDialogShown() = 0;

  // Handles the result of the dialog being shown.
  virtual void HandleDialogResult(DialogResult result) = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_
