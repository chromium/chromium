// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"

class Profile;

// The Ansible software configuration is shown to let the user know that an
// Ansible playbook is being applied and their app might take longer than
// usual to launch. This is specifically implemented in a way such that it
// doesn't need to know about which GuestID it's configuring for.
class CrostiniAnsibleSoftwareConfigView
    : public views::BubbleDialogDelegateView,
      public crostini::AnsibleManagementService::Observer {
  METADATA_HEADER(CrostiniAnsibleSoftwareConfigView,
                  views::BubbleDialogDelegateView)

 public:
  CrostiniAnsibleSoftwareConfigView(const CrostiniAnsibleSoftwareConfigView&) =
      delete;
  CrostiniAnsibleSoftwareConfigView& operator=(
      const CrostiniAnsibleSoftwareConfigView&) = delete;
  ~CrostiniAnsibleSoftwareConfigView() override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;

  std::u16string GetSubtextLabelStringForTesting();
  std::u16string GetProgressLabelStringForTesting();

  // AnsibleManagementService::Observer
  void OnAnsibleSoftwareConfigurationStarted(
      const guest_os::GuestId& container_id) override;
  void OnAnsibleSoftwareConfigurationProgress(
      const guest_os::GuestId& container_id,
      const std::vector<std::string>& status_lines) override;
  void OnAnsibleSoftwareConfigurationFinished(
      const guest_os::GuestId& container_id,
      bool success) override;

  explicit CrostiniAnsibleSoftwareConfigView(Profile* profile,
                                             guest_os::GuestId container_id);

 private:
  enum class State {
    CONFIGURING,
    ERROR,
    ERROR_OFFLINE,
  };

  std::u16string GetWindowTitleForState(State state);

  void OnStateChanged();
  std::u16string GetSubtextLabel() const;

  State state_ = State::CONFIGURING;

  raw_ptr<views::Label> subtext_label_ = nullptr;
  raw_ptr<views::Label> progress_label_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  base::FilePath default_container_ansible_filepath_;

  raw_ptr<Profile> profile_;

  guest_os::GuestId container_id_;
  std::u16string container_name_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
