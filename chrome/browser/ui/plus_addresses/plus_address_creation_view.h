// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_

#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

enum class PlusAddressViewButtonType { kCancel = 0, kConfirm = 1, kClose = 2 };

// An interface for orchestrating plus address creation UI.
class PlusAddressCreationView {
 public:
  // Updates the view to either show the plus address in the bottomsheet and
  // enable the OK button or show an error message.
  virtual void ShowReserveResult(
      const PlusProfileOrError& maybe_plus_profile) = 0;
  // Either closes the UI or shows an error message.
  virtual void ShowConfirmResult(
      const PlusProfileOrError& maybe_plus_profile) = 0;
  // Returns whether the Confirm button can be pressed.
  virtual bool GetConfirmButtonEnabledForTesting() const = 0;
  // Simulates a click on the `type` of button.
  // TODO: crbug.com/1467623 - Remove after migrating to Kombucha tests.
  virtual void ClickButtonForTesting(PlusAddressViewButtonType type) = 0;
  // Returns the text shown on the Plus Address label.
  virtual std::u16string GetPlusAddressLabelTextForTesting() const = 0;
  // Checks that the loading indicator is showing.
  virtual bool ShowsLoadingIndicatorForTesting() const = 0;
  // Blocks until either ShowReserveResult or ShowConfirmResult are called.
  virtual void WaitUntilResultShownForTesting() = 0;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_
