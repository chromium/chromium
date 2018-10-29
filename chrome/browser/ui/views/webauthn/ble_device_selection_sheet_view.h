// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_DEVICE_SELECTION_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_DEVICE_SELECTION_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/ble_device_hover_list_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to pick the Bluetooth security key to which they wish to connect to
// in order to service the Web Authentication request.
class BleDeviceSelectionSheetView : public AuthenticatorRequestSheetView,
                                    public BleDeviceHoverListModel::Delegate {
 public:
  explicit BleDeviceSelectionSheetView(
      std::unique_ptr<AuthenticatorBleDeviceSelectionSheetModel> model);
  ~BleDeviceSelectionSheetView() override;

 private:
  AuthenticatorBleDeviceSelectionSheetModel* model() {
    return static_cast<AuthenticatorBleDeviceSelectionSheetModel*>(
        AuthenticatorRequestSheetView::model());
  }

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  // BleDeviceHoverListModel::Delegate:
  void OnItemSelected(base::StringPiece authenticator_id) override;

  DISALLOW_COPY_AND_ASSIGN(BleDeviceSelectionSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_DEVICE_SELECTION_SHEET_VIEW_H_
