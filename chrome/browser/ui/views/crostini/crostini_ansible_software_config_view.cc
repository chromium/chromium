// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/network_service_instance.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

CrostiniAnsibleSoftwareConfigView*
    g_crostini_ansible_software_configuration_view = nullptr;

}  // namespace

namespace crostini {

void ShowCrostiniAnsibleSoftwareConfigView(Profile* profile) {
  if (!g_crostini_ansible_software_configuration_view) {
    g_crostini_ansible_software_configuration_view =
        new CrostiniAnsibleSoftwareConfigView(profile);
    views::DialogDelegate::CreateDialogWidget(
        g_crostini_ansible_software_configuration_view, nullptr, nullptr);
  }

  g_crostini_ansible_software_configuration_view->GetWidget()->Show();
}

void CloseCrostiniAnsibleSoftwareConfigViewForTesting() {
  if (g_crostini_ansible_software_configuration_view) {
    g_crostini_ansible_software_configuration_view->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
  g_crostini_ansible_software_configuration_view = nullptr;
}

}  // namespace crostini

bool CrostiniAnsibleSoftwareConfigView::Accept() {
  if (state_ == State::ERROR_OFFLINE) {
    state_ = State::CONFIGURING;
    OnStateChanged();

    ansible_management_service_->ConfigureDefaultContainer(base::DoNothing());
    return false;
  }
  DCHECK_EQ(state_, State::ERROR);
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

void CrostiniAnsibleSoftwareConfigView::
    OnAnsibleSoftwareConfigurationStarted() {}

void CrostiniAnsibleSoftwareConfigView::OnAnsibleSoftwareConfigurationFinished(
    bool success) {
  DCHECK_EQ(state_, State::CONFIGURING);

  if (!success) {
    if (content::GetNetworkConnectionTracker()->IsOffline())
      state_ = State::ERROR_OFFLINE;
    else
      state_ = State::ERROR;

    OnStateChanged();
    return;
  }
  // TODO(crbug.com/1005774): We should preferably add another ClosedReason
  // instead of passing kUnspecified.
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

std::u16string
CrostiniAnsibleSoftwareConfigView::GetSubtextLabelStringForTesting() {
  return subtext_label_->GetText();
}

// static
CrostiniAnsibleSoftwareConfigView*
CrostiniAnsibleSoftwareConfigView::GetActiveViewForTesting() {
  return g_crostini_ansible_software_configuration_view;
}

CrostiniAnsibleSoftwareConfigView::CrostiniAnsibleSoftwareConfigView(
    Profile* profile)
    : ansible_management_service_(
          crostini::AnsibleManagementService::GetForProfile(profile)) {
  ansible_management_service_->AddObserver(this);

  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::CONTROL));

  auto subtext_label = std::make_unique<views::Label>();
  subtext_label->SetMultiLine(true);
  subtext_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtext_label_ = AddChildView(std::move(subtext_label));

  // Add infinite progress bar.
  // TODO(crbug.com/1000173): add progress reporting and display text above
  // progress bar indicating current process.
  auto progress_bar = std::make_unique<views::ProgressBar>();
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar->SetValue(-1);
  progress_bar_ = AddChildView(std::move(progress_bar));

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CROSTINI_ANSIBLE_SOFTWARE_CONFIG);
  OnStateChanged();
}

CrostiniAnsibleSoftwareConfigView::~CrostiniAnsibleSoftwareConfigView() {
  ansible_management_service_->RemoveObserver(this);
  g_crostini_ansible_software_configuration_view = nullptr;
}

// static
std::u16string CrostiniAnsibleSoftwareConfigView::GetWindowTitleForState(
    State state) {
  switch (state) {
    case State::CONFIGURING:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL);
    case State::ERROR:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL);
    case State::ERROR_OFFLINE:
      return l10n_util::GetStringFUTF16(
          IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_LABEL,
          ui::GetChromeOSDeviceName());
  }
}

void CrostiniAnsibleSoftwareConfigView::OnStateChanged() {
  SetTitle(GetWindowTitleForState(state_));
  progress_bar_->SetVisible(state_ == State::CONFIGURING);
  subtext_label_->SetText(GetSubtextLabel());
  SetButtons(state_ == State::CONFIGURING
                 ? ui::DIALOG_BUTTON_NONE
                 : (state_ == State::ERROR
                        ? ui::DIALOG_BUTTON_OK
                        : ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));
  // The cancel button, even when present, always uses the default text.
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 state_ == State::ERROR
                     ? l10n_util::GetStringUTF16(IDS_APP_OK)
                     : l10n_util::GetStringUTF16(
                           IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_RETRY_BUTTON));
  DialogModelChanged();
  if (GetWidget())
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

BEGIN_METADATA(CrostiniAnsibleSoftwareConfigView,
               views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SubtextLabel)
END_METADATA
