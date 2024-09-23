// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_signals_consent/consent_dialog_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

using device_signals::prefs::kDeviceSignalsConsentReceived;

namespace {
std::unique_ptr<ConsentRequester>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<ConsentRequester>> storage;
  return storage.get();
}

ui::ImageModel GetIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

}  // namespace

std::unique_ptr<ui::DialogModel>
ConsentDialogCoordinator::CreateDeviceSignalsConsentDialogModel() {
  ui::DialogModel::Builder dialog_builder;
  return dialog_builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_DEVICE_SIGNALS_CONSENT_DIALOG_TITLE))
      .SetIcon(GetIcon())
      .AddOkButton(
          base::BindOnce(&ConsentDialogCoordinator::OnConsentDialogAccept,
                         weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_DEVICE_SIGNALS_CONSENT_DIALOG_PROCEED_BUTTON))
              .SetId(kDeviceSignalsConsentOkButtonElementId))
      .AddCancelButton(
          base::BindOnce(&ConsentDialogCoordinator::OnConsentDialogCancel,
                         weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_DEVICE_SIGNALS_CONSENT_DIALOG_CANCEL_BUTTON))
              .SetId(kDeviceSignalsConsentCancelButtonElementId))
      .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
      .AddParagraph(ui::DialogModelLabel(GetDialogBodyText()))
      .SetCloseActionCallback(
          base::BindOnce(&ConsentDialogCoordinator::OnConsentDialogClose,
                         weak_ptr_factory_.GetWeakPtr()))
      .Build();
}

// static
std::unique_ptr<ConsentRequester> ConsentRequester::CreateConsentRequester(
    Profile* profile) {
  std::unique_ptr<ConsentRequester>& test_instance = *GetTestInstanceStorage();
  if (test_instance) {
    return std::move(test_instance);
  }
  if (!profile) {
    return nullptr;
  }
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    return nullptr;
  }
  return std::make_unique<ConsentDialogCoordinator>(browser, profile);
}

// static
void ConsentRequester::SetConsentRequesterForTest(
    std::unique_ptr<ConsentRequester> consent_requester) {
  DCHECK(consent_requester);
  *GetTestInstanceStorage() = std::move(consent_requester);
}

ConsentDialogCoordinator::ConsentDialogCoordinator(Browser* browser,
                                                   Profile* profile)
    : browser_(browser), profile_(profile) {}

ConsentDialogCoordinator::~ConsentDialogCoordinator() {
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    dialog_widget_ = nullptr;
  }
}

void ConsentDialogCoordinator::RequestConsent(RequestConsentCallback callback) {
  pref_observer_.Init(profile_->GetPrefs());
  pref_observer_.Add(
      device_signals::prefs::kDeviceSignalsConsentReceived,
      base::BindRepeating(&ConsentDialogCoordinator::OnConsentPreferenceUpdated,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  Show();
}

std::u16string ConsentDialogCoordinator::GetDialogBodyText() {
  std::optional<std::string> manager =
      chrome::GetAccountManagerIdentity(profile_);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = chrome::GetDeviceManagerIdentity();
  }
  return (manager && manager.value().length())
             ? l10n_util::GetStringFUTF16(
                   IDS_DEVICE_SIGNALS_CONSENT_DIALOG_BODY_TEXT,
                   base::UTF8ToUTF16(manager.value()))
             : l10n_util::GetStringUTF16(
                   IDS_DEVICE_SIGNALS_CONSENT_DIALOG_DEFAULT_BODY_TEXT);
}

void ConsentDialogCoordinator::Show() {
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    if (!dialog_widget_->IsVisible()) {
      dialog_widget_->Show();
    }
    return;
  }
  base::RecordAction(base::UserMetricsAction("DeviceSignalsConsent_Shown"));
  dialog_widget_ = chrome::ShowBrowserModal(
      browser_, CreateDeviceSignalsConsentDialogModel());
}

void ConsentDialogCoordinator::OnConsentDialogAccept() {
  base::RecordAction(base::UserMetricsAction("DeviceSignalsConsent_Accepted"));
  profile_->GetPrefs()->SetBoolean(kDeviceSignalsConsentReceived, true);
}

void ConsentDialogCoordinator::OnConsentDialogCancel() {
  base::RecordAction(base::UserMetricsAction("DeviceSignalsConsent_Cancelled"));
  profiles::CloseProfileWindows(profile_);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void ConsentDialogCoordinator::OnConsentDialogClose() {
  if (dialog_widget_->closed_reason() ==
      views::Widget::ClosedReason::kEscKeyPressed) {
    base::RecordAction(
        base::UserMetricsAction("DeviceSignalsConsent_EscPressed"));
    OnConsentDialogCancel();
  }
}

void ConsentDialogCoordinator::OnConsentPreferenceUpdated(
    RequestConsentCallback callback) {
  if (profile_->GetPrefs()->GetBoolean(kDeviceSignalsConsentReceived)) {
    if (dialog_widget_) {
      dialog_widget_->CloseWithReason(
          views::Widget::ClosedReason::kAcceptButtonClicked);
      dialog_widget_ = nullptr;
    }
    callback.Run();
  }
}
