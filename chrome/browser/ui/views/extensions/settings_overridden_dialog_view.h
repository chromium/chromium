// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_H_

#include <memory>

#include "base/optional.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog that displays a warning to the user that their settings have been
// overridden by an extension.
class SettingsOverriddenDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(SettingsOverriddenDialogView);
  explicit SettingsOverriddenDialogView(
      std::unique_ptr<SettingsOverriddenDialogController> controller);
  SettingsOverriddenDialogView(const SettingsOverriddenDialogView&) = delete;
  SettingsOverriddenDialogView& operator=(const SettingsOverriddenDialogView&) =
      delete;
  ~SettingsOverriddenDialogView() override;

  // Displays the dialog with the given |parent|.
  void Show(gfx::NativeWindow parent);

 private:
  // Notifies the |controller_| of the |result|.
  void NotifyControllerOfResult(
      SettingsOverriddenDialogController::DialogResult result);

  // The result of the dialog; set when notifying the controller.
  base::Optional<SettingsOverriddenDialogController::DialogResult> result_;

  std::unique_ptr<SettingsOverriddenDialogController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_H_
