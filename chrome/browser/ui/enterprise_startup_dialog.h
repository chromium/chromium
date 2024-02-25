// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ENTERPRISE_STARTUP_DIALOG_H_
#define CHROME_BROWSER_UI_ENTERPRISE_STARTUP_DIALOG_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace policy {

// A dialog shown when there is non-trivial work that has to be finished
// before any Chrome window can be opened during startup. This dialog is only
// enabled by enterprise policy. For example, cloud policy enrollment or forced
// upgrade.
class EnterpriseStartupDialog {
 public:
  // Callback when dialog is closed.
  // |was_accepted| is true iff user confirmed the dialog. False if user
  // canceled the dialog.
  // |can_show_browser_window| is true if dialog is dismissed automatically once
  // the non-trivial work is finished and browser window can be displayed.
  // Otherwise, it's false. For example, user close the dialog or
  // click 'Relaunch Chrome' button on the dialog.
  using DialogResultCallback =
      base::OnceCallback<void(bool was_accepted, bool can_show_browser_window)>;

  EnterpriseStartupDialog(const EnterpriseStartupDialog&) = delete;
  EnterpriseStartupDialog& operator=(const EnterpriseStartupDialog&) = delete;

  virtual ~EnterpriseStartupDialog() = default;

  // Show the dialog. Please note that the dialog won't contain any
  // useful content until |Display*()| is called.
  static std::unique_ptr<EnterpriseStartupDialog> CreateAndShowDialog(
      DialogResultCallback callback);

  // Display |information| with a throbber. Changes the content of dialog
  // without re-opening it.
  virtual void DisplayLaunchingInformationWithThrobber(
      const std::u16string& information) = 0;
  // Display |error_message| with an error icon. Show confirm button with
  // value |accept_button| if provided. Changes the content of dialog without
  // re-opening it.
  virtual void DisplayErrorMessage(
      const std::u16string& error_message,
      const std::optional<std::u16string>& accept_button) = 0;
  // Return true if dialog is being displayed.
  virtual bool IsShowing() = 0;

 protected:
  EnterpriseStartupDialog() = default;
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_ENTERPRISE_STARTUP_DIALOG_H_
