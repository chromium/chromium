// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"

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
#include "ui/events/event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

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

class AppMenuSearchBarView::SearchField : public views::Textfield,
                                          public ui::EventObserver {
  METADATA_HEADER(SearchField, views::Textfield)

 public:
  using views::Textfield::OnEvent;

  SearchField();
  SearchField(const SearchField&) = delete;
  SearchField& operator=(const SearchField&) = delete;
  ~SearchField() override;

  void SetLeadingIcon(LeadingIcon icon);

  [[nodiscard]] base::CallbackListSubscription AddBackPressedCallback(
      base::RepeatingClosure callback);

  bool is_active() const { return is_active_; }

  void HandleKeyEvent(ui::KeyEvent* event);

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // views::Textfield:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  void SetTextfieldFocused(bool focused);

  // Notifies `back_pressed_callbacks_`, then clears the query.
  void OnBackPressed();

  bool is_active_ = false;
  LeadingIcon leading_icon_ = LeadingIcon::kSearch;
  raw_ptr<views::ImageView> search_icon_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  std::unique_ptr<views::EventMonitor> event_monitor_;
  base::RepeatingClosureList back_pressed_callbacks_;
};

AppMenuSearchBarView::SearchField::SearchField() {
  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  int left_inset = kIconPadding + icon_size + icon_text_spacing;

  SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  SetPlaceholderTextColorId(ui::kColorTextfieldForegroundPlaceholder);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kVerticalPadding, left_inset, kVerticalPadding, kIconPadding)));
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetCursorEnabled(true);

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
  back_button->SetProperty(views::kElementIdentifierKey, kBackButtonElementId);
  views::InstallCircleHighlightPathGenerator(back_button.get());
  back_button_ = AddChildView(std::move(back_button));

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

void AppMenuSearchBarView::SearchField::AddedToWidget() {
  views::Textfield::AddedToWidget();
  SetTextfieldFocused(true);
  if (GetWidget()) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, GetWidget()->GetNativeWindow(), {ui::EventType::kKeyPressed});
  }
}

void AppMenuSearchBarView::SearchField::RemovedFromWidget() {
  event_monitor_.reset();
  views::Textfield::RemovedFromWidget();
}

void AppMenuSearchBarView::SearchField::Layout(PassKey) {
  LayoutSuperclass<views::Textfield>(this);
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  const gfx::Rect leading_bounds(kIconPadding, (height() - icon_size) / 2,
                                 icon_size, icon_size);

  search_icon_->SetBoundsRect(leading_bounds);
  back_button_->SetBoundsRect(leading_bounds);
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
  SetText(std::u16string());
  SetTextfieldFocused(true);
}

bool AppMenuSearchBarView::SearchField::OnMousePressed(
    const ui::MouseEvent& event) {
  SetTextfieldFocused(true);
  return views::Textfield::OnMousePressed(event);
}

void AppMenuSearchBarView::SearchField::SetTextfieldFocused(bool focused) {
  is_active_ = focused;
  SetCursorEnabled(focused);
  if (focused) {
    RequestFocus();
  } else if (GetFocusManager()) {
    GetFocusManager()->ClearFocus();
  }
  SchedulePaint();
}

void AppMenuSearchBarView::SearchField::OnEvent(const ui::Event& event) {
  if (event.IsKeyEvent()) {
    ui::KeyEvent key_event = *event.AsKeyEvent();
    HandleKeyEvent(&key_event);
  }
}

void AppMenuSearchBarView::SearchField::HandleKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Only handle keys when the search bar is active/focused.
  if (!is_active_) {
    return;
  }

  const ui::KeyboardCode key_code = event->key_code();

  // Down Arrow clears focus from search bar and lets menu navigate down.
  if (key_code == ui::VKEY_DOWN) {
    SetTextfieldFocused(false);
    return;
  }

  // Leave menu action/dismissal keys for MenuController.
  if (key_code == ui::VKEY_ESCAPE || key_code == ui::VKEY_RETURN ||
      key_code == ui::VKEY_UP) {
    return;
  }

  // Typing and cursor editing (Backspace, Delete, Left/Right arrow).
  ui::TextEditCommand command = GetCommandForKeyEvent(*event);
  if (command != ui::TextEditCommand::INVALID_COMMAND) {
    ExecuteTextEditCommand(command);
  } else if (event->GetCharacter() != 0) {
    InsertChar(*event);
  }

  event->StopPropagation();
  event->SetHandled();
}

BEGIN_METADATA(AppMenuSearchBarView, SearchField)
END_METADATA

AppMenuSearchBarView::AppMenuSearchBarView() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  search_field_ = AddChildView(std::make_unique<SearchField>());
  search_field_->SetProperty(views::kElementIdentifierKey,
                             kSearchFieldElementId);
  search_field_->SetProperty(views::kMarginsKey,
                             ChromeLayoutProvider::Get()->GetInsetsMetric(
                                 INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN));
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

bool AppMenuSearchBarView::is_active_for_testing() const {
  return search_field_->is_active();
}

void AppMenuSearchBarView::HandleKeyEvent(ui::KeyEvent* event) {
  search_field_->HandleKeyEvent(event);
}

BEGIN_METADATA(AppMenuSearchBarView)
END_METADATA
