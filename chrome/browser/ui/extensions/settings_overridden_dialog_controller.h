// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_

#include <optional>
#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

namespace image_fetcher {
class ImageFetcher;
}

// The controller for the SettingsOverriddenDialog. This class is responsible
// for both providing the display information (ShowParams) as well as handling
// the result of the dialog (i.e., the user input).
class SettingsOverriddenDialogController {
 public:
  // Describes the visual elements (texts, icon) of a selectable option in the
  // dialog.
  struct SettingOption {
    // Lightweight descriptor for an option's icon, resolved when building the
    // dialog view: no icon (falls back to a generic icon), a bundled theme
    // resource ID, or a remote favicon URL fetched asynchronously.
    using IconSource = std::variant<std::monostate, int, GURL>;

    SettingOption();
    ~SettingOption();
    SettingOption(const SettingOption& other);
    SettingOption& operator=(const SettingOption& other);
    SettingOption(SettingOption&& other);
    SettingOption& operator=(SettingOption&& other);

    // The primary text of the option.
    std::u16string text;

    // Additional subtext describing the option.
    std::u16string description;

    // Describes how to load the option's icon when the dialog view is built.
    IconSource icon_source;
  };

  // A struct describing the contents to be displayed in the dialog.
  struct ShowParams {
    ShowParams();
    ~ShowParams();
    ShowParams(const ShowParams& params);
    ShowParams(std::u16string dialog_title,
               std::u16string dialog_message,
               const gfx::VectorIcon* icon);

    std::u16string dialog_title;
    std::u16string message;

    // The icon to display, if any. If non-null, the VectorIcon should have
    // all its colors fully specified; otherwise a placehold grey color will
    // be used.
    raw_ptr<const gfx::VectorIcon> icon = nullptr;

    // If present, the dialog will present a radio-button-based choice as to
    // whether to use the new setting or previous, rather than a
    // keep-or-go-back presentation. The dialog will issue the same dialog
    // result, either way.
    std::optional<SettingOption> previous_setting;
    std::optional<SettingOption> new_setting;
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

  // Returns the image fetcher used to asynchronously load option favicons, or
  // nullptr if network icon fetching is unavailable.
  virtual image_fetcher::ImageFetcher* GetImageFetcher() = 0;

  // Notifies the controller that the dialog will be shown.
  virtual void OnDialogWillBeShown() = 0;

  // Handles the result of the dialog being shown.
  virtual void HandleDialogResult(DialogResult result) = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_CONTROLLER_H_
