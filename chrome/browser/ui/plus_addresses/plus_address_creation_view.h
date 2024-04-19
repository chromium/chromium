// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_

#include "components/plus_addresses/plus_address_types.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}  // namespace content

namespace plus_addresses {

enum class PlusAddressViewButtonType { kCancel = 0, kConfirm = 1, kClose = 2 };

// An interface for orchestrating plus address creation UI.
class PlusAddressCreationView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPlusAddressDescriptionTextElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPlusAddressErrorTextElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPlusAddressConfirmButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPlusAddressCancelButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPlusAddressSuggestedEmailElementId);

  // Updates the view to either show the plus address in the bottom sheet and
  // enable the OK button or show an error message.
  virtual void ShowReserveResult(
      const PlusProfileOrError& maybe_plus_profile) = 0;
  // Either closes the UI or shows an error message.
  virtual void ShowConfirmResult(
      const PlusProfileOrError& maybe_plus_profile) = 0;
  // Navigates to the link shown in the dialog's description.
  virtual void OpenSettingsLink(content::WebContents* web_contents) = 0;
  // Navigates to the link shown in error report instructions.
  virtual void OpenErrorReportLink(content::WebContents* web_contents) = 0;
  // Hides the button for refreshing the plus address.
  virtual void HideRefreshButton() = 0;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_H_
