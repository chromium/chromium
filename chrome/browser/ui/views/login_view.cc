// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/login_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/textfield_layout.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"

namespace {

constexpr int kHeaderColumnSetId = 0;
constexpr int kFieldsColumnSetId = 1;

// Adds a row to |layout| and puts a Label in it.
void AddHeaderLabel(views::GridLayout* layout,
                    const base::string16& text,
                    int text_style) {
  auto label = std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL,
                                              text_style);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  layout->StartRow(views::GridLayout::kFixedSize, kHeaderColumnSetId);
  layout->AddView(std::move(label));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginView, public:

LoginView::LoginView(const base::string16& authority,
                     const base::string16& explanation,
                     LoginHandler::LoginModelData* login_model_data)
    : http_auth_manager_(login_model_data ? login_model_data->model : nullptr) {
  // TODO(tapted): When Harmony is default, this should be removed and left up
  // to textfield_layout.h to decide.
  constexpr int kMessageWidth = 320;
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::CONTROL)));

  // Initialize the Grid Layout Manager used for this dialog box.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kHeaderColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kFixed, kMessageWidth,
                        0);
  AddHeaderLabel(layout, authority, views::style::STYLE_PRIMARY);
  if (!explanation.empty())
    AddHeaderLabel(layout, explanation, views::style::STYLE_SECONDARY);
  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));

  ConfigureTextfieldStack(layout, kFieldsColumnSetId);
  username_field_ = AddFirstTextfieldRow(
      layout, l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD),
      kFieldsColumnSetId);
  password_field_ = AddTextfieldRow(
      layout, l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD),
      kFieldsColumnSetId);
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  if (http_auth_manager_) {
    http_auth_manager_->SetObserverAndDeliverCredentials(
        this, login_model_data->form);
  }
}

LoginView::~LoginView() {
  if (http_auth_manager_)
    http_auth_manager_->DetachObserver(this);
}

const base::string16& LoginView::GetUsername() const {
  return username_field_->GetText();
}

const base::string16& LoginView::GetPassword() const {
  return password_field_->GetText();
}

views::View* LoginView::GetInitiallyFocusedView() {
  return username_field_;
}

///////////////////////////////////////////////////////////////////////////////
// LoginView, views::View, password_manager::HttpAuthObserver overrides:

void LoginView::OnAutofillDataAvailable(const base::string16& username,
                                        const base::string16& password) {
  if (username_field_->GetText().empty()) {
    username_field_->SetText(username);
    password_field_->SetText(password);
    username_field_->SelectAll(true);
  }
}

void LoginView::OnLoginModelDestroying() {
  http_auth_manager_ = nullptr;
}

const char* LoginView::GetClassName() const {
  return "LoginView";
}
