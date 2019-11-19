// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ble_device_selection_sheet_view.h"

#include <utility>

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

BleDeviceSelectionSheetView::BleDeviceSelectionSheetView(
    std::unique_ptr<AuthenticatorBleDeviceSelectionSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

BleDeviceSelectionSheetView::~BleDeviceSelectionSheetView() = default;

std::unique_ptr<views::View>
BleDeviceSelectionSheetView::BuildStepSpecificContent() {
  auto device_selection_view = std::make_unique<views::View>();
  device_selection_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  auto device_list_view =
      std::make_unique<HoverListView>(std::make_unique<BleDeviceHoverListModel>(
          &model()->dialog_model()->saved_authenticators(), this));

  auto bottom_suggestion_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_BLE_DEVICE_SELECTION_REMINDER_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_DISABLED);
  bottom_suggestion_label->SetMultiLine(true);
  bottom_suggestion_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  device_selection_view->AddChildView(device_list_view.release());
  device_selection_view->AddChildView(bottom_suggestion_label.release());

  return device_selection_view;
}

void BleDeviceSelectionSheetView::OnItemSelected(
    base::StringPiece authenticator_id) {
  model()->dialog_model()->InitiatePairingDevice(authenticator_id);
}
