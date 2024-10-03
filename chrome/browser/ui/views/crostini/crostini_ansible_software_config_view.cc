// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

bool CrostiniAnsibleSoftwareConfigView::Accept() {
  if (state_ == State::ERROR_OFFLINE) {
    state_ = State::CONFIGURING;
    OnStateChanged();

    crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
        ->RetryConfiguration(container_id_);
    return false;
  }
  DCHECK_EQ(state_, State::ERROR);
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->CompleteConfiguration(container_id_, false);
  return true;
}

bool CrostiniAnsibleSoftwareConfigView::Cancel() {
  if (state_ == State::CONFIGURING) {
    // Cancel anything running/waiting on this specific configuration task.
    crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
        ->CancelConfiguration(container_id_);
  }
  // Always close.
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->CompleteConfiguration(container_id_, false);
  return true;
}

std::u16string CrostiniAnsibleSoftwareConfigView::GetSubtextLabel() const {
  switch (state_) {
    case State::CONFIGURING:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT);
    case State::ERROR:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_SUBTEXT);
    case State::ERROR_OFFLINE:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_SUBTEXT);
  }
}

std::u16string
CrostiniAnsibleSoftwareConfigView::GetSubtextLabelStringForTesting() {
  return subtext_label_->GetText();
}

std::u16string
CrostiniAnsibleSoftwareConfigView::GetProgressLabelStringForTesting() {
  return progress_label_->GetText();
}

CrostiniAnsibleSoftwareConfigView::CrostiniAnsibleSoftwareConfigView(
    Profile* profile,
    guest_os::GuestId container_id)
    : profile_(profile), container_id_(container_id) {
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));

  // Allow the dialog to be moved around.
  set_draggable(true);

  auto subtext_label = std::make_unique<views::Label>();
  subtext_label->SetMultiLine(true);
  subtext_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtext_label_ = AddChildView(std::move(subtext_label));

  // Add infinite progress bar.
  auto progress_bar = std::make_unique<views::ProgressBar>();
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar->SetValue(-1);
  progress_bar_ = AddChildView(std::move(progress_bar));

  // Adds text below the infinite progress bar stating the last action.
  // Currently can't really display the full progress since Ansible doesn't
  // expose this by default w/o specific playbook hacks.
  auto progress_label = std::make_unique<views::Label>();
  progress_label->SetMultiLine(true);
  progress_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  progress_label_ = AddChildView(std::move(progress_label));
  default_container_ansible_filepath_ = profile->GetPrefs()->GetFilePath(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath);

  container_name_ = base::UTF8ToUTF16(container_id.container_name);
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile)
      ->AddObserver(this);
  OnStateChanged();
}

CrostiniAnsibleSoftwareConfigView::~CrostiniAnsibleSoftwareConfigView() =
    default;

std::u16string CrostiniAnsibleSoftwareConfigView::GetWindowTitleForState(
    State state) {
  switch (state) {
    case State::CONFIGURING:
      return l10n_util::GetStringUTF16(
                 IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL) +
             u": " + container_name_;
    case State::ERROR:
      return l10n_util::GetStringUTF16(
                 IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL) +
             u": " + container_name_;
    case State::ERROR_OFFLINE:
      return l10n_util::GetStringFUTF16(
                 IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_LABEL,
                 ui::GetChromeOSDeviceName()) +
             u": " + container_name_;
  }
}

void CrostiniAnsibleSoftwareConfigView::OnAnsibleSoftwareConfigurationStarted(
    const guest_os::GuestId& container_id) {}

void CrostiniAnsibleSoftwareConfigView::OnAnsibleSoftwareConfigurationProgress(
    const guest_os::GuestId& container_id,
    const std::vector<std::string>& status_lines) {
  // Pass if this isn't for the current dialog.
  LOG(ERROR) << "Progress: " << status_lines.back();
  if (container_id != container_id_)
    return;
  progress_label_->SetText(base::UTF8ToUTF16(status_lines.back()));
  OnStateChanged();
}

void CrostiniAnsibleSoftwareConfigView::OnAnsibleSoftwareConfigurationFinished(
    const guest_os::GuestId& container_id,
    bool success) {
  // Pass if this isn't for the current dialog.
  if (container_id != container_id_)
    return;

  DCHECK_EQ(state_, State::CONFIGURING);
  if (!success) {
    if (content::GetNetworkConnectionTracker()->IsOffline())
      state_ = State::ERROR_OFFLINE;
    else
      state_ = State::ERROR;

    OnStateChanged();
    return;
  }
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  crostini::AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->CompleteConfiguration(container_id_, true);
}

void CrostiniAnsibleSoftwareConfigView::OnStateChanged() {
  SetTitle(GetWindowTitleForState(state_));
  progress_bar_->SetVisible(state_ == State::CONFIGURING);
  subtext_label_->SetText(GetSubtextLabel());
  SetButtons(
      state_ == State::CONFIGURING
          ? static_cast<int>(ui::mojom::DialogButton::kCancel)
          : (state_ == State::ERROR
                 ? static_cast<int>(ui::mojom::DialogButton::kOk)
                 : static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)));
  // The cancel button, even when present, always uses the default text.
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_APP_OK));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  DialogModelChanged();
  if (GetWidget())
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

BEGIN_METADATA(CrostiniAnsibleSoftwareConfigView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SubtextLabel)
END_METADATA
