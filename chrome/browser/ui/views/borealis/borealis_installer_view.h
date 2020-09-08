// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

class Profile;

// The front end for the Borealis installation process, works closely with
// "chrome/browser/chromeos/borealis/borealis_installer.h".
class BorealisInstallerView : public views::DialogDelegateView,
                              public borealis::BorealisInstaller::Observer {
 public:
  using InstallingState = borealis::BorealisInstaller::InstallingState;

  explicit BorealisInstallerView(Profile* profile);

  // Disallow copy and assign.
  BorealisInstallerView(const BorealisInstallerView&) = delete;
  BorealisInstallerView& operator=(const BorealisInstallerView&) = delete;

  static BorealisInstallerView* GetActiveViewForTesting();

  // views::DialogDelegateView implementation.
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // borealis::BorealisInstaller::Observer implementation.
  void OnStateUpdated(
      borealis::BorealisInstaller::InstallingState new_state) override;
  void OnProgressUpdated(double fraction_complete) override;
  void OnInstallationEnded(
      borealis::BorealisInstaller::InstallationResult result) override;
  void OnCancelInitiated() override {}

  // Public for testing purposes.
  base::string16 GetPrimaryMessage() const;
  base::string16 GetSecondaryMessage() const;

  void SetInstallingStateForTesting(InstallingState new_state);

 private:
  class TitleLabel;
  enum class State {
    kConfirmInstall,  // Waiting for user to start installation.
    kInstalling,      // Installation in progress.
    kCompleted,       // Installation process completed.
    kError,           // Something unexpected happened.
  };

  ~BorealisInstallerView() override;

  // Returns the dialog buttons that should be displayed, based on the current
  // |state_| and error |reason_| (if relevant).
  int GetCurrentDialogButtons() const;
  // Returns the label for a dialog |button|, based on the current |state_|
  // and error |reason_| (if relevant).
  base::string16 GetCurrentDialogButtonLabel(ui::DialogButton button) const;

  void OnStateUpdated();

  // views::DialogDelegateView implementation.
  void AddedToWidget() override;

  void SetPrimaryMessageLabel();
  void SetSecondaryMessageLabel();
  void SetImage();

  void StartInstallation();

  base::string16 app_name_;
  views::Label* primary_message_label_ = nullptr;
  views::Label* secondary_message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;
  views::Label* installation_progress_message_label_ = nullptr;
  views::BoxLayout* lower_container_layout_ = nullptr;
  views::ImageView* big_image_ = nullptr;

  borealis::BorealisInstaller* borealis_installer_ = nullptr;
  State state_ = State::kConfirmInstall;
  InstallingState installing_state_ = InstallingState::kInactive;
  base::Optional<borealis::BorealisInstaller::InstallationResult> result_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_
