// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caret_browsing_dialog_delegate.h"

#include <memory>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/events/ash/keyboard_capability.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
void CaretBrowsingDialogDelegate::Show(gfx::NativeWindow parent_window,
                                       PrefService* pref_service) {
  // When the window closes, it will delete itself.
  constrained_window::CreateBrowserModalDialogViews(
      new CaretBrowsingDialogDelegate(pref_service), parent_window)
      ->Show();
}

CaretBrowsingDialogDelegate::CaretBrowsingDialogDelegate(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  base::RecordAction(
      base::UserMetricsAction("Accessibility.CaretBrowsing.ShowDialog"));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<ui::KeyboardCode> key =
      ash::AccessibilityManager::Get()->GetCaretBrowsingActionKey();
  std::u16string key_string;
  std::u16string key_name;
  if (!key) {
    key_string = l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_F7);
  } else {
    switch (*key) {
      case ui::VKEY_F7:
        key_string = l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_F7);
        break;
      case ui::VKEY_BRIGHTNESS_DOWN:
        key_name =
            l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_BRIGHTNESS_DOWN);
        key_string = l10n_util::GetStringFUTF16(
            IDS_CARET_BROWSING_KEY_SEARCH_MODIFIER, {key_name});
        break;
      case ui::VKEY_BRIGHTNESS_UP:
        key_name =
            l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_BRIGHTNESS_UP);
        key_string = l10n_util::GetStringFUTF16(
            IDS_CARET_BROWSING_KEY_SEARCH_MODIFIER, {key_name});
        break;
      case ui::VKEY_VOLUME_MUTE:
        key_name = l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_MUTE);
        key_string = l10n_util::GetStringFUTF16(
            IDS_CARET_BROWSING_KEY_SEARCH_MODIFIER, {key_name});
        break;
      case ui::VKEY_MEDIA_PLAY_PAUSE:
        key_name = l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_PLAY_PAUSE);
        key_string = l10n_util::GetStringFUTF16(
            IDS_CARET_BROWSING_KEY_SEARCH_MODIFIER, {key_name});
        break;
      default:
        key_name = l10n_util::GetStringUTF16(IDS_CARET_BROWSING_KEY_F7);
        key_string = l10n_util::GetStringFUTF16(
            IDS_CARET_BROWSING_KEY_SEARCH_MODIFIER, {key_name});
    }
  }
  std::u16string message_text =
      l10n_util::GetStringFUTF16(IDS_ENABLE_CARET_BROWSING_INFO, {key_string});
#else
  std::u16string message_text =
      l10n_util::GetStringUTF16(IDS_ENABLE_CARET_BROWSING_INFO);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* message_label = AddChildView(std::make_unique<views::Label>(
      message_text, views::style::CONTEXT_DIALOG_BODY_TEXT));
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetMultiLine(true);

  do_not_ask_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_CARET_BROWSING_DO_NOT_ASK)));

  SetTitle(l10n_util::GetStringUTF16(IDS_ENABLE_CARET_BROWSING_TITLE));

  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_ENABLE_CARET_BROWSING_TURN_ON));

  SetShowCloseButton(false);

  const auto on_accept = [](PrefService* pref_service,
                            views::Checkbox* do_not_ask_checkbox) {
    base::RecordAction(
        base::UserMetricsAction("Accessibility.CaretBrowsing.AcceptDialog"));
    pref_service->SetBoolean(prefs::kCaretBrowsingEnabled, true);
    if (do_not_ask_checkbox->GetChecked()) {
      base::RecordAction(
          base::UserMetricsAction("Accessibility.CaretBrowsing.DoNotAsk"));
      pref_service->SetBoolean(prefs::kShowCaretBrowsingDialog, false);
    }
  };
  SetAcceptCallback(
      base::BindOnce(on_accept, pref_service_, do_not_ask_checkbox_));

  const auto on_cancel = []() {
    base::RecordAction(
        base::UserMetricsAction("Accessibility.CaretBrowsing.CancelDialog"));
  };
  SetCancelCallback(base::BindOnce(on_cancel));

  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

CaretBrowsingDialogDelegate::~CaretBrowsingDialogDelegate() = default;

BEGIN_METADATA(CaretBrowsingDialogDelegate)
END_METADATA
