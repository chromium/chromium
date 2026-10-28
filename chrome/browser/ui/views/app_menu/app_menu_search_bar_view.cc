// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kVerticalPadding = 6;
constexpr int kCornerRadius = 8;
constexpr int kIconPadding = 12;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuSearchBarView,
                                      kSearchFieldElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuSearchBarView,
                                      kSearchIconElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuSearchBarView,
                                      kBackButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuSearchBarView,
                                      kClearButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AppMenuSearchBarView,
                                      kSeparatorElementId);

class AppMenuSearchBarView::SearchField : public views::Textfield,
                                          public views::TextfieldController {
  METADATA_HEADER(SearchField, views::Textfield)

 public:
  SearchField();
  SearchField(const SearchField&) = delete;
  SearchField& operator=(const SearchField&) = delete;
  ~SearchField() override;

  void SetLeadingIcon(LeadingIcon icon);

  [[nodiscard]] base::CallbackListSubscription AddBackPressedCallback(
      base::RepeatingClosure callback);

  // views::Textfield:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  // Notifies `back_pressed_callbacks_`, then clears the query.
  void OnBackPressed();

  // Empties the query and keeps the search bar focused.
  void ClearQuery();

  // Shows the clear button only while there is a query.
  void UpdateClearButtonVisibility();

  LeadingIcon leading_icon_ = LeadingIcon::kSearch;
  raw_ptr<views::ImageView> search_icon_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::ImageButton> clear_button_ = nullptr;
  base::RepeatingClosureList back_pressed_callbacks_;
  base::CallbackListSubscription text_changed_subscription_;
};

AppMenuSearchBarView::SearchField::SearchField() {
  set_controller(this);

  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  // Leaves room for the leading icon and the clear button.
  int horizontal_inset = kIconPadding + icon_size + icon_text_spacing;

  SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  SetPlaceholderTextColorId(ui::kColorTextfieldForegroundPlaceholder);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kVerticalPadding, horizontal_inset)));
  SetBackgroundColor(SK_ColorTRANSPARENT);

  auto search_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled()
              ? vector_icons::kSearchIcon
              : vector_icons::kSearchChromeRefreshOldIcon,
          ui::kColorIcon, icon_size));
  search_icon->SetCanProcessEventsWithinSubtree(false);
  search_icon->SetProperty(views::kElementIdentifierKey, kSearchIconElementId);
  search_icon_ = AddChildView(std::move(search_icon));

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SearchField::OnBackPressed, base::Unretained(this)),
      features::IsRoundedIconsEnabled() ? vector_icons::kArrowBackIcon
                                        : vector_icons::kArrowBackOldIcon,
      icon_size);
  back_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetVisible(false);
  back_button->SetFocusBehavior(FocusBehavior::NEVER);
  back_button->SetProperty(views::kElementIdentifierKey, kBackButtonElementId);
  views::InstallCircleHighlightPathGenerator(back_button.get());
  back_button_ = AddChildView(std::move(back_button));

  auto clear_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SearchField::ClearQuery, base::Unretained(this)),
      features::IsRoundedIconsEnabled()
          ? vector_icons::kCloseIcon
          : vector_icons::kCloseChromeRefreshOldIcon,
      icon_size);
  clear_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CLEAR_SEARCH));
  clear_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLEAR_SEARCH));
  clear_button->SetVisible(false);
  clear_button->SetFocusBehavior(FocusBehavior::NEVER);
  clear_button->SetProperty(views::kElementIdentifierKey,
                            kClearButtonElementId);
  views::InstallCircleHighlightPathGenerator(clear_button.get());
  clear_button_ = AddChildView(std::move(clear_button));
  text_changed_subscription_ = AddTextChangedCallback(base::BindRepeating(
      &SearchField::UpdateClearButtonVisibility, base::Unretained(this)));

  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(ink_drop,
                                               /*highlight_on_hover=*/true,
                                               /*highlight_on_focus=*/true);
  ink_drop->SetBaseColor(ui::kColorSysStateHoverOnSubtle);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);
}

AppMenuSearchBarView::SearchField::~SearchField() = default;

gfx::Size AppMenuSearchBarView::SearchField::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::Textfield::CalculatePreferredSize(available_size);
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  size.set_height(std::max(icon_size, GetFontList().GetHeight()) +
                  GetInsets().height());
  return size;
}

void AppMenuSearchBarView::SearchField::Layout(PassKey) {
  LayoutSuperclass<views::Textfield>(this);
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  const gfx::Rect leading_bounds(kIconPadding, (height() - icon_size) / 2,
                                 icon_size, icon_size);

  search_icon_->SetBoundsRect(leading_bounds);
  back_button_->SetBoundsRect(leading_bounds);
  clear_button_->SetBoundsRect(gfx::Rect(width() - kIconPadding - icon_size,
                                         leading_bounds.y(), icon_size,
                                         icon_size));
}

void AppMenuSearchBarView::SearchField::SetLeadingIcon(LeadingIcon icon) {
  if (leading_icon_ == icon) {
    return;
  }
  leading_icon_ = icon;
  search_icon_->SetVisible(icon == LeadingIcon::kSearch);
  back_button_->SetVisible(icon == LeadingIcon::kBack);
}

base::CallbackListSubscription
AppMenuSearchBarView::SearchField::AddBackPressedCallback(
    base::RepeatingClosure callback) {
  return back_pressed_callbacks_.Add(std::move(callback));
}

void AppMenuSearchBarView::SearchField::OnBackPressed() {
  back_pressed_callbacks_.Notify();
  ClearQuery();
}

void AppMenuSearchBarView::SearchField::ClearQuery() {
  SetText(std::u16string());
  RequestFocus();
}

void AppMenuSearchBarView::SearchField::UpdateClearButtonVisibility() {
  clear_button_->SetVisible(!GetText().empty());
}

bool AppMenuSearchBarView::SearchField::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyPressed &&
      key_event.key_code() == ui::VKEY_ESCAPE && !key_event.IsShiftDown() &&
      !key_event.IsControlDown() && !key_event.IsAltDown() &&
      !key_event.IsCommandDown() && !GetText().empty()) {
    ClearQuery();
    return true;
  }
  return false;
}

BEGIN_METADATA(AppMenuSearchBarView, SearchField)
END_METADATA

AppMenuSearchBarView::AppMenuSearchBarView() {
  const auto* provider = ChromeLayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  search_field_ = AddChildView(std::make_unique<SearchField>());
  search_field_->SetProperty(views::kElementIdentifierKey,
                             kSearchFieldElementId);
  search_field_->SetProperty(
      views::kMarginsKey,
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN));

  // The separator spans the full width of the menu.
  auto* separator = AddChildView(std::make_unique<views::Separator>());
  separator->SetProperty(views::kElementIdentifierKey, kSeparatorElementId);
  separator->SetOrientation(views::Separator::Orientation::kHorizontal);
  separator->SetColorId(ui::kColorMenuSeparator);
  separator->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          provider->GetDistanceMetric(
              DISTANCE_ACTION_APP_MENU_SEARCH_SEPARATOR_BOTTOM_MARGIN),
          0));
}

AppMenuSearchBarView::~AppMenuSearchBarView() = default;

void AppMenuSearchBarView::SetLeadingIcon(LeadingIcon icon) {
  search_field_->SetLeadingIcon(icon);
}

std::u16string_view AppMenuSearchBarView::GetText() const {
  return search_field_->GetText();
}

void AppMenuSearchBarView::SetText(std::u16string_view text) {
  search_field_->SetText(text);
}

base::CallbackListSubscription AppMenuSearchBarView::AddTextChangedCallback(
    views::PropertyChangedCallback callback) {
  return search_field_->AddTextChangedCallback(std::move(callback));
}

base::CallbackListSubscription AppMenuSearchBarView::AddBackPressedCallback(
    base::RepeatingClosure callback) {
  return search_field_->AddBackPressedCallback(std::move(callback));
}

BEGIN_METADATA(AppMenuSearchBarView)
END_METADATA
