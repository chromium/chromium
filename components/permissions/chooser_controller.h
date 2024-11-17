// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CHOOSER_CONTROLLER_H_
#define COMPONENTS_PERMISSIONS_CHOOSER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace permissions {

// Subclass ChooserController to implement a chooser, which has some
// introductory text and a list of options that users can pick one of.
// Your subclass must define the set of options users can pick from;
// the actions taken after users select an item or press the 'Cancel'
// button or the chooser is closed.
// After Select/Cancel/Close is called, this object is destroyed and
// calls back into it are not allowed.
class ChooserController {
 public:
  explicit ChooserController(std::u16string title);

  ChooserController(const ChooserController&) = delete;
  ChooserController& operator=(const ChooserController&) = delete;

  virtual ~ChooserController();

  // Since the set of options can change while the UI is visible an
  // implementation should register a view to observe changes.
  class View {
   public:
    // Called after the options list is initialized for the first time.
    // OnOptionsInitialized should only be called once.
    virtual void OnOptionsInitialized() = 0;

    // Called after GetOption(index) has been added to the options and the
    // newly added option is the last element in the options list. Calling
    // GetOption(index) from inside a call to OnOptionAdded will see the
    // added string since the options have already been updated.
    virtual void OnOptionAdded(size_t index) = 0;

    // Called when GetOption(index) is no longer present, and all later
    // options have been moved earlier by 1 slot. Calling GetOption(index)
    // from inside a call to OnOptionRemoved will NOT see the removed string
    // since the options have already been updated.
    virtual void OnOptionRemoved(size_t index) = 0;

    // Called when the option at |index| has been updated.
    virtual void OnOptionUpdated(size_t index) = 0;

    // Called when the device adapter is turned on or off.
    virtual void OnAdapterEnabledChanged(bool enabled) = 0;

    // Called when the platform level device permission is changed.
    // Currently only needed on macOS.
    virtual void OnAdapterAuthorizationChanged(bool authorized);

    // Called when refreshing options is in progress or complete.
    virtual void OnRefreshStateChanged(bool refreshing) = 0;

   protected:
    virtual ~View() = default;
  };

  // Returns the text to be displayed in the chooser title.
  // Note that this is only called once, and there is no way to update the title
  // for a given instance of ChooserController.
  std::u16string GetTitle() const;

  // Returns whether the chooser needs to show an icon before the text.
  // For WebBluetooth, it is a signal strength icon.
  virtual bool ShouldShowIconBeforeText() const;

  // Returns whether the chooser needs to show a help button.
  virtual bool ShouldShowHelpButton() const;

  // Returns whether the chooser needs to show a button to re-scan for devices.
  virtual bool ShouldShowReScanButton() const;

  // Returns whether the chooser allows multiple items to be selected.
  virtual bool AllowMultipleSelection() const;

  // Returns whether the chooser needs to show a select-all checkbox.
  virtual bool ShouldShowSelectAllCheckbox() const;

  // Returns the text to be displayed in the chooser when there are no options.
  virtual std::u16string GetNoOptionsText() const = 0;

  // Returns the label for OK button.
  virtual std::u16string GetOkButtonLabel() const = 0;

  // Returns the label for Cancel button.
  virtual std::u16string GetCancelButtonLabel() const;

  // Returns the label for SelectAll checkbox.
  virtual std::u16string GetSelectAllCheckboxLabel() const;

  // Returns the label for the throbber shown while options are initializing or
  // a re-scan is in progress.
  virtual std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const = 0;

  // Returns whether both OK and Cancel buttons are enabled.
  //
  // For chooser used in Web APIs such as WebBluetooth, WebUSB,
  // WebSerial, etc., the OK button is only enabled when there is at least
  // one device listed in the chooser, because user needs to be able to select
  // a device to grant access permission in these APIs.
  //
  // For permission prompt used in Bluetooth scanning Web API, the two buttons
  // represent Allow and Block, and should always be enabled so that user can
  // make their permission decision.
  virtual bool BothButtonsAlwaysEnabled() const;

  // Returns whether table view should always be disabled.
  //
  // For permission prompt used in Bluetooth scanning Web API, the table is
  // used for displaying device names, and user doesn't need to select a device
  // from the table, so it should always be disabled.
  virtual bool TableViewAlwaysDisabled() const;

  // The number of options users can pick from. For example, it can be
  // the number of USB/Bluetooth device names which are listed in the
  // chooser so that users can grant permission.
  virtual size_t NumOptions() const = 0;

  // The signal strength level (0-4 inclusive) of the device at |index|, which
  // is used to retrieve the corresponding icon to be displayed before the
  // text. Returns -1 if no icon should be shown.
  virtual int GetSignalStrengthLevel(size_t index) const;

  // The |index|th option string which is listed in the chooser.
  virtual std::u16string GetOption(size_t index) const = 0;

  // Returns if the |index|th option is connected.
  // This function returns false by default.
  virtual bool IsConnected(size_t index) const;

  // Returns if the |index|th option is paired.
  // This function returns false by default.
  virtual bool IsPaired(size_t index) const;

  // Refresh the list of options.
  virtual void RefreshOptions();

  // These three functions are called just before this object is destroyed:

  // Called when the user selects elements from the dialog. |indices| contains
  // the indices of the selected elements.
  virtual void Select(const std::vector<size_t>& indices) = 0;

  // Called when the user presses the 'Cancel' button in the dialog.
  virtual void Cancel() = 0;

  // Called when the user clicks outside the dialog or the dialog otherwise
  // closes without the user taking an explicit action.
  virtual void Close() = 0;

  // Open help center URL.
  virtual void OpenHelpCenterUrl() const = 0;

  // Provide help information when the adapter is off.
  virtual void OpenAdapterOffHelpUrl() const;

  // Navigate user to preferences in order to acquire Bluetooth permission.
  virtual void OpenPermissionPreferences() const;

  // Return whether the chooser needs to show Bluetooth adapter view.
  virtual bool ShouldShowAdapterOffView() const;

  // Return the message id for Bluetooth adapter being off.
  virtual int GetAdapterOffMessageId() const;

  // Return the message id of the link text for turning Bluetooth adapter on.
  virtual int GetTurnAdapterOnLinkTextMessageId() const;

  // Return whether the chooser needs to show Bluetooth unauthorized view.
  virtual bool ShouldShowAdapterUnauthorizedView() const;

  // Return the message id for Bluetooth access unauthorized.
  virtual int GetBluetoothUnauthorizedMessageId() const;

  // Return the label of the link text for authotizing Bluetooth access.
  virtual int GetAuthorizeBluetoothLinkTextMessageId() const;

  // Only one view may be registered at a time.
  void set_view(View* view) { view_ = view; }
  View* view() const { return view_; }

 protected:
  void set_title_for_testing(const std::u16string& title) { title_ = title; }

 private:
  std::u16string title_;
  raw_ptr<View, DanglingUntriaged> view_ = nullptr;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CHOOSER_CONTROLLER_H_
