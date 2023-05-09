// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_signals_consent/device_signals_consent_dialog_coordinator.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace {

ui::ImageModel GetIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

void OnConsentResponseReceived(Profile* profile, bool consent_received) {
  if (consent_received) {
    profile->GetPrefs()->SetBoolean(
        device_signals::prefs::kDeviceSignalsConsentReceived, true);
    return;
  }

  profiles::CloseProfileWindows(profile);

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
}

std::unique_ptr<ui::DialogModel> CreateDeviceSignalsConsentDialogModel(
    Profile* profile) {
  ui::DialogModel::Builder dialog_builder;
  return dialog_builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_DEVICE_SIGNALS_CONSENT_DIALOG_TITLE))
      .SetIcon(GetIcon())
      .AddOkButton(
          base::BindOnce(&OnConsentResponseReceived, profile, true),
          ui::DialogModelButton::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_DEVICE_SIGNALS_CONSENT_DIALOG_PROCEED_BUTTON)))
      .AddCancelButton(
          base::BindOnce(&OnConsentResponseReceived, profile, false),
          ui::DialogModelButton::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_DEVICE_SIGNALS_CONSENT_DIALOG_CANCEL_BUTTON)))
      .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_NONE)
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
          IDS_DEVICE_SIGNALS_CONSENT_DIALOG_BODY_TEXT, u"example.com")))
      .Build();
}

}  // namespace

// static
views::Widget* DeviceSignalsConsentDialogCoordinator::ShowDialog(
    Browser* browser) {
  return chrome::ShowBrowserModal(
      browser, CreateDeviceSignalsConsentDialogModel(browser->profile()));
}
