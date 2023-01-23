// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class BoxLayout;
class Label;
class ProgressBar;
}  // namespace views

// The front end for the Bruschetta installation process, works closely with
// "chrome/browser/ash/bruschetta/bruschetta_installer.h".
class BruschettaInstallerView
    : public views::DialogDelegateView,
      public bruschetta::BruschettaInstaller::Observer,
      public ash::ColorModeObserver {
 public:
  METADATA_HEADER(BruschettaInstallerView);

  using InstallerState = bruschetta::BruschettaInstaller::State;

  static void Show(Profile* profile, const guest_os::GuestId& guest_id);

  explicit BruschettaInstallerView(Profile* profile,
                                   guest_os::GuestId guest_id);

  // Disallow copy and assign.
  BruschettaInstallerView(const BruschettaInstallerView&) = delete;
  BruschettaInstallerView& operator=(const BruschettaInstallerView&) = delete;

  static BruschettaInstallerView* GetActiveViewForTesting();

  // views::DialogDelegateView implementation.
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

  // bruschetta::BruschettaInstaller::Observer implementation.
  void StateChanged(InstallerState state) override;
  void Error(bruschetta::BruschettaInstallResult error) override;

  // Public for testing purposes.
  std::u16string GetPrimaryMessage() const;
  std::u16string GetSecondaryMessage() const;
  void OnInstallationEnded();

  // Instead of creating a real one the view will use this one, letting tests
  // inject an installer.
  void set_installer_for_testing(
      std::unique_ptr<bruschetta::BruschettaInstaller> installer) {
    installer_ = std::move(installer);
  }

 private:
  class TitleLabel;
  enum class State {
    kConfirmInstall,  // Waiting for user to start installation.
    kInstalling,      // Installation in progress.
    kFailed,          // Installation process failed.
    // Note: No succeeded state since we close the installer upon success.
  };

  ~BruschettaInstallerView() override;

  // Returns the dialog buttons that should be displayed, based on the current
  // |state_|.
  int GetCurrentDialogButtons() const;

  // Returns the label for a dialog |button|, based on the current |state_|.
  std::u16string GetCurrentDialogButtonLabel(ui::DialogButton button) const;

  // views::DialogDelegateView implementation.
  void AddedToWidget() override;

  // ash::ColorModeObserver overrides.
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void SetPrimaryMessageLabel();
  void SetSecondaryMessageLabel();

  void StartInstallation();
  void OnStateUpdated();

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<views::Label> primary_message_label_ = nullptr;
  raw_ptr<views::Label> secondary_message_label_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::BoxLayout> lower_container_layout_ = nullptr;

  State state_ = State::kConfirmInstall;
  InstallerState installing_state_ = InstallerState::kInstallStarted;

  base::ScopedObservation<bruschetta::BruschettaInstaller,
                          bruschetta::BruschettaInstaller::Observer>
      observation_;

  std::unique_ptr<bruschetta::BruschettaInstaller> installer_;
  guest_os::GuestId guest_id_;
  bruschetta::BruschettaInstallResult error_ =
      bruschetta::BruschettaInstallResult::kUnknown;
  bool is_destroying_ = false;

  base::WeakPtrFactory<BruschettaInstallerView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_
