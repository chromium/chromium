// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"

namespace chromeos::mahi {

namespace {
// TODO(b:374172642): final numbers are TBD
constexpr int kMaxCharForCondensedView = 100;
constexpr int kMinCharForElucidation = 100;
constexpr int kMaxCharForElucidation = 1600;

// Whether the `text` is eligible for elucidation / simplication feature.
SelectedTextState IsTextEligibleForElucidation(const std::u16string& text) {
  if (text.empty()) {
    return SelectedTextState::kEmpty;
  }
  if (text.length() > kMaxCharForElucidation) {
    return SelectedTextState::kTooLong;
  } else if (text.length() < kMinCharForElucidation) {
    return SelectedTextState::kTooShort;
  }

  return SelectedTextState::kEligible;
}

// Whether a condensed mahi menu view should show on right clicking
// `selected_text` instead of a full size widget, to avoid possible collision
// with quick answer card.
// TODO(b:374172642): the check simply uses char count, while quick answer
// detects intent of selected text with async calls to ml-service. Let's
// re-visit this if collisions are reported.
bool ShouldShowMahiCondensedMenuView(const std::u16string& selected_text) {
  return !selected_text.empty() &&
         selected_text.length() <= kMaxCharForCondensedView;
}

}  // namespace

MahiMenuController::MahiMenuController(
    ReadWriteCardsUiController& read_write_cards_ui_controller)
    : read_write_cards_ui_controller_(read_write_cards_ui_controller) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // MahiMediaAppEventsProxy is initialized only in ash chrome.
  CHECK(chromeos::MahiMediaAppEventsProxy::Get());
  chromeos::MahiMediaAppEventsProxy::Get()->AddObserver(this);
#endif
}

MahiMenuController::~MahiMenuController() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(chromeos::MahiMediaAppEventsProxy::Get());
  chromeos::MahiMediaAppEventsProxy::Get()->RemoveObserver(this);
#endif
}

void MahiMenuController::OnContextMenuShown(Profile* profile) {}

void MahiMenuController::OnTextAvailable(const gfx::Rect& anchor_bounds,
                                         const std::string& selected_text,
                                         const std::string& surrounding_text) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!MahiManager::Get() || !MahiManager::Get()->IsEnabled()) {
    return;
  }
#endif

  // Only shows mahi menu for distillable pages or when the switch
  // `kUseFakeMahiManager` is enabled.
  if (!chromeos::MahiWebContentsManager::Get()->IsFocusedPageDistillable() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseFakeMahiManager)) {
    return;
  }

  const std::u16string selected_text_u16 = base::UTF8ToUTF16(selected_text);
  chromeos::MahiWebContentsManager::Get()->SetSelectedText(selected_text_u16);

  // If Pompano feature flag is enabled, uses the new logic to show mahi widget.
  if (features::IsPompanoEnabled()) {
    // If the selected text passes the check, we will show the condensed Mahi
    // view to avoid possible collision against the quick answer card.
    if (ShouldShowMahiCondensedMenuView(selected_text_u16)) {
      read_write_cards_ui_controller_->SetMahiUi(
          std::make_unique<MahiCondensedMenuView>());
      return;
    }

    menu_widget_ = MahiMenuView::CreateWidget(
        anchor_bounds, {.elucidation_eligiblity =
                            IsTextEligibleForElucidation(selected_text_u16)});
    // This enables tooltip without having to activate the text field.
    menu_widget_->SetNativeWindowProperty(
        views::TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
    menu_widget_->ShowInactive();
    return;
  }

  if (selected_text.empty()) {
    // Sets elucidation_eligibility = kUnknown to hide the elucidation button.
    menu_widget_ = MahiMenuView::CreateWidget(
        anchor_bounds, {.elucidation_eligiblity = SelectedTextState::kUnknown});
    menu_widget_->ShowInactive();
    return;
  }

  // If there is selected text, we will show the condensed Mahi view alongside
  // quick answers.
  read_write_cards_ui_controller_->SetMahiUi(
      std::make_unique<MahiCondensedMenuView>());
}

void MahiMenuController::OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) {
  if (!menu_widget_ || !menu_widget_->GetContentsView()) {
    return;
  }

  views::AsViewClass<MahiMenuView>(menu_widget_->GetContentsView())
      ->UpdateBounds(anchor_bounds);
}

void MahiMenuController::OnDismiss(bool is_other_command_executed) {
  if (menu_widget_ && !menu_widget_->IsActive()) {
    menu_widget_.reset();
  }

  read_write_cards_ui_controller_->RemoveMahiUi();
}

void MahiMenuController::OnPdfContextMenuShown(const gfx::Rect& anchor) {
  if (!MahiManager::Get() || !MahiManager::Get()->IsEnabled()) {
    return;
  }

  if (!MagicBoostState::Get()->ShouldShowHmrCard()) {
    return;
  }

  // kUnknown means hiding the elucidation button.
  SelectedTextState elucidation_eligiblity = SelectedTextState::kUnknown;
  if (features::IsPompanoEnabled()) {
    CHECK(chromeos::MahiMediaAppContentManager::Get());
    elucidation_eligiblity = IsTextEligibleForElucidation(base::UTF8ToUTF16(
        chromeos::MahiMediaAppContentManager::Get()->GetSelectedText()));
  }

  menu_widget_ = MahiMenuView::CreateWidget(
      anchor, {.elucidation_eligiblity = elucidation_eligiblity},
      MahiMenuView::Surface::kMediaApp);
  menu_widget_->ShowInactive();
}

void MahiMenuController::OnPdfContextMenuHide() {
  OnDismiss(/*is_other_command_executed=*/false);
}

bool MahiMenuController::IsFocusedPageDistillable() {
  if (is_distillable_for_testing_.has_value()) {
    return is_distillable_for_testing_.value();
  }

  return chromeos::MahiWebContentsManager::Get()->IsFocusedPageDistillable() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kUseFakeMahiManager);
}

void MahiMenuController::RecordPageDistillable() {
  // Records metric of whether the page is distillable when Mahi menu is
  // requested to show.
  base::UmaHistogramBoolean(kMahiContextMenuDistillableHistogram,
                            IsFocusedPageDistillable());
}

base::WeakPtr<MahiMenuController> MahiMenuController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos::mahi
