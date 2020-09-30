// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/cookie_info_view.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Normally, a textfield would eat gesture events in one of two ways:
//  - When active, it processes the events and marks the events as handled.
//    Even in single-line text fields, we do still need the ability to scroll
//    horizontally to see the entire text.
//  - When inactive, the events are not processed by the view, but are also not
//    routed up the view hierarchy.
//
// This class is identical to views::Textfield, but passes gesture events down
// to the containing ScrollView. When disabled, it refuses all input events,
// allowing for correct propagation.
//
// See crbug.com/1008806 for an example of how the dialog breaks without this
// change.
//
// TODO(crbug.com/1011082): Solve this in the general case.
class GestureScrollableTextfield : public views::Textfield {
 public:
  explicit GestureScrollableTextfield(views::ScrollView* scroll_parent)
      : scroll_parent_(scroll_parent),
        on_enabled_subscription_(AddEnabledChangedCallback(
            base::BindRepeating(&GestureScrollableTextfield::OnEnabledChanged,
                                base::Unretained(this)))) {}

 private:
  // views::Textfield:
  void OnGestureEvent(ui::GestureEvent* event) override {
    scroll_parent_->OnGestureEvent(event);
    Textfield::OnGestureEvent(event);
  }

  void OnEnabledChanged() { SetCanProcessEventsWithinSubtree(GetEnabled()); }

  views::ScrollView* const scroll_parent_;
  views::PropertyChangedSubscription on_enabled_subscription_;
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView() {
  constexpr int kLabelValuePadding = 96;

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const gfx::Insets& dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(0, dialog_insets.left(), 0,
                                     dialog_insets.right()));

  View* const contents = SetContents(std::make_unique<views::View>());
  views::GridLayout* layout =
      contents->SetLayoutManager(std::make_unique<views::GridLayout>());

  int three_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(provider->GetControlLabelGridAlignment(),
                        views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               kLabelValuePadding);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  for (const auto& cookie_property_and_label : {
           std::make_pair(CookieProperty::kName, IDS_COOKIES_COOKIE_NAME_LABEL),
           std::make_pair(CookieProperty::kContent,
                          IDS_COOKIES_COOKIE_CONTENT_LABEL),
           std::make_pair(CookieProperty::kDomain,
                          IDS_COOKIES_COOKIE_DOMAIN_LABEL),
           std::make_pair(CookieProperty::kPath, IDS_COOKIES_COOKIE_PATH_LABEL),
           std::make_pair(CookieProperty::kSendFor,
                          IDS_COOKIES_COOKIE_SENDFOR_LABEL),
           std::make_pair(CookieProperty::kCreated,
                          IDS_COOKIES_COOKIE_CREATED_LABEL),
           std::make_pair(CookieProperty::kExpires,
                          IDS_COOKIES_COOKIE_EXPIRES_LABEL),
       }) {
    property_textfields_[cookie_property_and_label.first] = AddTextfieldRow(
        three_column_layout_id, layout, cookie_property_and_label.second);
  }
  SetTextfieldColors();
}

CookieInfoView::~CookieInfoView() = default;

void CookieInfoView::SetCookie(const std::string& domain,
                               const net::CanonicalCookie& cookie) {
  const std::unordered_map<CookieProperty, base::string16> strings_map{
      {CookieProperty::kName, base::UTF8ToUTF16(cookie.Name())},
      {CookieProperty::kContent, base::UTF8ToUTF16(cookie.Value())},
      {CookieProperty::kDomain, base::UTF8ToUTF16(domain)},
      {CookieProperty::kPath, base::UTF8ToUTF16(cookie.Path())},
      {CookieProperty::kSendFor,
       l10n_util::GetStringUTF16(
           CookiesTreeModel::GetSendForMessageID(cookie))},
      {CookieProperty::kCreated,
       base::TimeFormatFriendlyDateAndTime(cookie.CreationDate())},
      {CookieProperty::kExpires,
       cookie.IsPersistent()
           ? base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())
           : l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_SESSION)}};

  for (const auto& p : strings_map)
    property_textfields_[p.first]->SetText(p.second);

  EnableCookieDisplay(true);
  Layout();
}

void CookieInfoView::ClearCookieDisplay() {
  for (const auto textfield_pair : property_textfields_) {
    textfield_pair.second->SetText(
        l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NONESELECTED));
  }
  EnableCookieDisplay(false);
}

void CookieInfoView::EnableCookieDisplay(bool enabled) {
  for (const auto textfield_pair : property_textfields_)
    textfield_pair.second->SetEnabled(enabled);
}

void CookieInfoView::OnThemeChanged() {
  SetTextfieldColors();
  views::ScrollView::OnThemeChanged();
}

void CookieInfoView::SetTextfieldColors() {
  auto* theme = GetNativeTheme();
  for (const auto textfield_pair : property_textfields_) {
    textfield_pair.second->SetBackgroundColor(
        theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground));
    textfield_pair.second->SetTextColor(
        theme->GetSystemColor(ui::NativeTheme::kColorId_DialogForeground));
  }
}

views::Textfield* CookieInfoView::AddTextfieldRow(int layout_id,
                                                  views::GridLayout* layout,
                                                  int label_message_id) {
  auto textfield = std::make_unique<GestureScrollableTextfield>(this);
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id));
  textfield->SetAssociatedLabel(label.get());
  layout->StartRow(views::GridLayout::kFixedSize, layout_id);
  layout->AddView(std::move(label));
  auto* textfield_ptr =
      layout->AddView(std::move(textfield), 2, 1, views::GridLayout::FILL,
                      views::GridLayout::CENTER);

  // Now that the Textfield is in the view hierarchy, it can be initialized.
  textfield_ptr->SetReadOnly(true);
  textfield_ptr->SetBorder(views::NullBorder());

  return textfield_ptr;
}
