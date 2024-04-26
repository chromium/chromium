// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_

#include "components/autofill/core/browser/field_types.h"

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of the Payment Request Dialog.

namespace payments {

enum class DialogViewID : int {
  VIEW_ID_NONE = autofill::MAX_VALID_FIELD_TYPE,
  CONTENT_VIEW,  // The main content view filled by each sheet

  // The following are views::Button (clickable).
  PAYMENT_SHEET_CONTACT_INFO_SECTION,
  PAYMENT_SHEET_SUMMARY_SECTION,
  PAYMENT_SHEET_PAYMENT_METHOD_SECTION,
  PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION,
  PAYMENT_SHEET_SHIPPING_OPTION_SECTION,
  PAYMENT_METHOD_ADD_CARD_BUTTON,
  PAYMENT_METHOD_ADD_SHIPPING_BUTTON,
  PAYMENT_METHOD_ADD_CONTACT_BUTTON,
  EDITOR_SAVE_BUTTON,
  PAY_BUTTON,
  CANCEL_BUTTON,
  BACK_BUTTON,
  CVC_PROMPT_CONFIRM_BUTTON,
  ADD_BILLING_ADDRESS_BUTTON,
  SAVE_ADDRESS_BUTTON,
  EDIT_ITEM_BUTTON,

  // The following are buttons that are displayed inline in the Payment Sheet
  // sections when no option is selected or available.
  PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON,
  PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON,
  PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON,
  PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON,

  // The following are StyledLabel objects.
  ORDER_SUMMARY_TOTAL_AMOUNT_LABEL,
  ORDER_SUMMARY_TOTAL_CURRENCY_LABEL,
  ORDER_SUMMARY_LINE_ITEM_1,
  ORDER_SUMMARY_LINE_ITEM_2,
  ORDER_SUMMARY_LINE_ITEM_3,
  DATA_SOURCE_LABEL,

  // This is the title used at the top of each sheet.
  SHEET_TITLE,

  // The following are Label objects.
  SHIPPING_OPTION_DESCRIPTION,
  SHIPPING_OPTION_AMOUNT,

  // Used in warning labels. This is unique at the scope of the each sheet.
  WARNING_LABEL,

  // Used in profile labels to annotate each line of the grouping.
  PROFILE_LABEL_LINE_1,
  PROFILE_LABEL_LINE_2,
  PROFILE_LABEL_LINE_3,
  PROFILE_LABEL_ERROR,

  // The following are views contained within the Payment Method Sheet.
  CONTACT_INFO_SHEET_LIST_VIEW,
  PAYMENT_METHOD_SHEET_LIST_VIEW,
  SHIPPING_ADDRESS_SHEET_LIST_VIEW,

  // Used in selectable rows. Each row in a view reuses this ID, but the ID is
  // unique at the scope of the parent row.
  CHECKMARK_VIEW,

  // The CVC text field in the unmask sheet.
  // TODO(crbug.com/40182225): Deprecated; remove usage.
  CVC_PROMPT_TEXT_FIELD,

  // The following are the ids for the individual sheets.
  CONTACT_INFO_EDITOR_SHEET,
  CREDIT_CARD_EDITOR_SHEET,
  ERROR_SHEET,
  ORDER_SUMMARY_SHEET,
  PAYMENT_REQUEST_SHEET,
  SHIPPING_ADDRESS_EDITOR_SHEET,

  // The sheet opens payment app window.
  PAYMENT_APP_OPENED_WINDOW_SHEET,

  // The icon displayed in the header for a payment app's window.
  PAYMENT_APP_HEADER_ICON,

  // The header view of a payment app's window.
  PAYMENT_APP_HEADER,

  // The scroll view of the payment sheet.
  PAYMENT_SHEET_SCROLL_VIEW,

  // NOTE: Keep these values last.
  // Used to offset the IDs of input fields, which gets added to the Autofill
  // type value they represent.
  INPUT_FIELD_TYPE_OFFSET,
  // Used to label the error labels with an offset, which gets added to
  // the Autofill type value they represent.
  ERROR_LABEL_OFFSET = INPUT_FIELD_TYPE_OFFSET + autofill::MAX_VALID_FIELD_TYPE,
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_
