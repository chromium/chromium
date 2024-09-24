// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_multi_source_picker_view.h"

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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace {

std::pair<std::unique_ptr<views::View>, HoverListView*> CreatePasskeyList(
    const std::optional<std::u16string>& title,
    const std::vector<int>& passkey_indices,
    AuthenticatorRequestDialogModel* dialog_model) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));
  if (title) {
    auto* label_container =
        container->AddChildView(std::make_unique<views::View>());
    label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    auto* label = label_container->AddChildView(std::make_unique<views::Label>(
        *title, views::style::CONTEXT_DIALOG_BODY_TEXT));
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  }
  HoverListView* control = container->AddChildView(
      std::make_unique<HoverListView>(std::make_unique<TransportHoverListModel>(
          dialog_model, passkey_indices)));
  return std::make_pair(std::move(container), control);
}

}  // namespace

AuthenticatorMultiSourcePickerView::AuthenticatorMultiSourcePickerView(
    AuthenticatorMultiSourcePickerSheetModel* model) {
  constexpr int kPaddingInBetweenPasskeyLists = 20;
  auto layout = std::make_unique<views::BoxLayout>();
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(kPaddingInBetweenPasskeyLists);
  SetLayoutManager(std::move(layout));

  std::optional<std::u16string> secondary_passkeys_label;
  if (!model->primary_passkey_indices().empty()) {
    secondary_passkeys_label =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_OTHER_DEVICES_LABEL);
    std::pair<std::unique_ptr<views::View>, HoverListView*> primary_list =
        CreatePasskeyList(model->primary_passkeys_label(),
                          model->primary_passkey_indices(),
                          model->dialog_model());
    AddChildView(std::move(primary_list.first));
    primary_passkeys_control_ = primary_list.second;
  }

  if (!model->secondary_passkey_indices().empty()) {
    std::pair<std::unique_ptr<views::View>, HoverListView*> secondary_list =
        CreatePasskeyList(secondary_passkeys_label,
                          model->secondary_passkey_indices(),
                          model->dialog_model());
    AddChildView(std::move(secondary_list.first));
    secondary_passkeys_control_ = secondary_list.second;
  }
}

AuthenticatorMultiSourcePickerView::~AuthenticatorMultiSourcePickerView() =
    default;

void AuthenticatorMultiSourcePickerView::RequestFocus() {
  if (primary_passkeys_control_) {
    primary_passkeys_control_->RequestFocus();
    return;
  }
  if (secondary_passkeys_control_) {
    secondary_passkeys_control_->RequestFocus();
  }
}

BEGIN_METADATA(AuthenticatorMultiSourcePickerView)
END_METADATA
