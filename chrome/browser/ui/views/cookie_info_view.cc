// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/cookie_info_view.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/table_layout.h"
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
  METADATA_HEADER(GestureScrollableTextfield);
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

  const raw_ptr<views::ScrollView> scroll_parent_;
  base::CallbackListSubscription on_enabled_subscription_;
};

BEGIN_METADATA(GestureScrollableTextfield, views::Textfield)
END_METADATA

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView() {
  constexpr int kLabelValuePadding = 96;

  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const gfx::Insets& dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right())));

  View* const contents = SetContents(std::make_unique<views::View>());
  views::TableLayout* layout =
      contents->SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kLabelValuePadding)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

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
    property_textfields_[cookie_property_and_label.first] =
        AddTextfieldRow(layout, cookie_property_and_label.second);
  }
}

CookieInfoView::~CookieInfoView() = default;

void CookieInfoView::SetCookie(const std::string& domain,
                               const net::CanonicalCookie& cookie) {
  const std::unordered_map<CookieProperty, std::u16string> strings_map{
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
  const auto* color_provider = GetColorProvider();
  for (const auto textfield_pair : property_textfields_) {
    textfield_pair.second->SetBackgroundColor(
        color_provider->GetColor(ui::kColorDialogBackground));
    textfield_pair.second->SetTextColor(
        color_provider->GetColor(ui::kColorDialogForeground));
  }
}

views::Textfield* CookieInfoView::AddTextfieldRow(views::TableLayout* layout,
                                                  int label_message_id) {
  layout->AddRows(1, views::TableLayout::kFixedSize);
  auto* label = contents()->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id)));
  auto* textfield = contents()->AddChildView(
      std::make_unique<GestureScrollableTextfield>(this));
  textfield->SetAccessibleName(label);
  textfield->SetReadOnly(true);
  textfield->SetBorder(views::NullBorder());

  return textfield;
}

BEGIN_METADATA(CookieInfoView, views::ScrollView)
END_METADATA
