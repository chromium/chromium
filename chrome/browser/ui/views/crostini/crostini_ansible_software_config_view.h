// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Profile;

// The Ansible software configuration is shown to let the user know that an
// Ansible playbook is being applied and their app might take longer than
// usual to launch.
class CrostiniAnsibleSoftwareConfigView
    : public views::BubbleDialogDelegateView,
      public crostini::AnsibleManagementService::Observer {
 public:
  METADATA_HEADER(CrostiniAnsibleSoftwareConfigView);

  // views::DialogDelegateView:
  bool Accept() override;

  // crostini::AnsibleManagementService::Observer:
  void OnAnsibleSoftwareConfigurationStarted() override;
  void OnAnsibleSoftwareConfigurationFinished(bool success) override;

  std::u16string GetSubtextLabelStringForTesting();

  static CrostiniAnsibleSoftwareConfigView* GetActiveViewForTesting();

  explicit CrostiniAnsibleSoftwareConfigView(Profile* profile);

 private:
  enum class State {
    CONFIGURING,
    ERROR,
    ERROR_OFFLINE,
  };

  static std::u16string GetWindowTitleForState(State state);

  void OnStateChanged();
  std::u16string GetSubtextLabel() const;

  State state_ = State::CONFIGURING;
  crostini::AnsibleManagementService* ansible_management_service_ = nullptr;

  views::Label* subtext_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  ~CrostiniAnsibleSoftwareConfigView() override;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAnsibleSoftwareConfigView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_VIEW_H_
