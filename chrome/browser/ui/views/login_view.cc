// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/login_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"

LoginView::LoginView(const std::u16string& authority,
                     const std::u16string& explanation,
                     LoginHandler::LoginModelData* login_model_data)
    : http_auth_manager_(login_model_data ? login_model_data->model.get()
                                          : nullptr) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  auto* authority_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  authority_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  auto* authority_label =
      authority_container->AddChildView(std::make_unique<views::Label>(
          authority, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  authority_label->SetMultiLine(true);
  constexpr int kMessageWidth = 320;
  authority_label->SetMaximumWidth(kMessageWidth);
  authority_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  authority_label->SetAllowCharacterBreak(true);
  if (!explanation.empty()) {
    auto* explanation_label = authority_container->AddChildView(
        std::make_unique<views::Label>(explanation, views::style::CONTEXT_LABEL,
                                       views::style::STYLE_SECONDARY));
    explanation_label->SetMultiLine(true);
    explanation_label->SetMaximumWidth(kMessageWidth);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  auto* fields_container =
      AddChildView(std::make_unique<views::TableLayoutView>());
  fields_container
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(
          views::TableLayout::kFixedSize,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     ChromeLayoutProvider::Get()->GetDistanceMetric(
                         DISTANCE_CONTROL_LIST_VERTICAL))
      .AddRows(1, views::TableLayout::kFixedSize);
  auto* username_label =
      fields_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD),
          views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_field_ =
      fields_container->AddChildView(std::make_unique<views::Textfield>());
  username_field_->GetViewAccessibility().SetName(*username_label);
  auto* password_label =
      fields_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD),
          views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_field_ =
      fields_container->AddChildView(std::make_unique<views::Textfield>());
  password_field_->GetViewAccessibility().SetName(*password_label);
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  if (http_auth_manager_) {
    http_auth_manager_->SetObserverAndDeliverCredentials(
        this, *login_model_data->form);
  }
}

LoginView::~LoginView() {
  if (http_auth_manager_)
    http_auth_manager_->DetachObserver(this);
}

const std::u16string& LoginView::GetUsername() const {
  return username_field_->GetText();
}

const std::u16string& LoginView::GetPassword() const {
  return password_field_->GetText();
}

views::View* LoginView::GetInitiallyFocusedView() {
  return username_field_;
}

void LoginView::OnAutofillDataAvailable(const std::u16string& username,
                                        const std::u16string& password) {
  if (username_field_->GetText().empty()) {
    username_field_->SetText(username);
    password_field_->SetText(password);
    username_field_->SelectAll(true);
  }
}

void LoginView::OnLoginModelDestroying() {
  http_auth_manager_ = nullptr;
}

BEGIN_METADATA(LoginView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Username)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Password)
END_METADATA
