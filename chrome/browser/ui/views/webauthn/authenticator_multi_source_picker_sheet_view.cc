// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_multi_source_picker_sheet_view.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace {

std::unique_ptr<views::View> CreatePasskeyList(
    const absl::optional<std::u16string>& title,
    const std::vector<int>& passkey_indices,
    const base::span<const AuthenticatorRequestDialogModel::Mechanism> mechs) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  auto label_container = std::make_unique<views::View>();
  label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  if (title) {
    auto label = std::make_unique<views::Label>(
        *title, views::style::CONTEXT_DIALOG_BODY_TEXT);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label_container->AddChildView(std::move(label));
    container->AddChildView(std::move(label_container));
  }
  container->AddChildView(std::make_unique<HoverListView>(
      std::make_unique<TransportHoverListModel>(mechs, passkey_indices)));
  return container;
}

}  // namespace

AuthenticatorMultiSourcePickerSheetView::
    AuthenticatorMultiSourcePickerSheetView(
        std::unique_ptr<AuthenticatorMultiSourcePickerSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorMultiSourcePickerSheetView::
    ~AuthenticatorMultiSourcePickerSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorMultiSourcePickerSheetView::BuildStepSpecificContent() {
  constexpr int kPaddingInBetweenPasskeyLists = 20;
  auto* sheet_model =
      static_cast<AuthenticatorMultiSourcePickerSheetModel*>(model());

  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(kPaddingInBetweenPasskeyLists);

  absl::optional<std::u16string> secondary_passkeys_label;
  if (!sheet_model->primary_passkey_indices().empty()) {
    secondary_passkeys_label =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_OTHER_DEVICES_LABEL),
    container->AddChildView(
        CreatePasskeyList(sheet_model->primary_passkeys_label(),
                          sheet_model->primary_passkey_indices(),
                          sheet_model->dialog_model()->mechanisms()));
  }

  container->AddChildView(CreatePasskeyList(
      secondary_passkeys_label, sheet_model->secondary_passkey_indices(),
      sheet_model->dialog_model()->mechanisms()));

  return std::make_pair(std::move(container), AutoFocus::kYes);
}
