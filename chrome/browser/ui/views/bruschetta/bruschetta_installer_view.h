// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class Label;
class ProgressBar;
}  // namespace views

// The front end for the Bruschetta installation process, works closely with
// "chrome/browser/ash/bruschetta/bruschetta_installer.h".
class BruschettaInstallerView
    : public views::DialogDelegateView,
      public bruschetta::BruschettaInstaller::Observer,
      public ash::ColorModeObserver {
  METADATA_HEADER(BruschettaInstallerView, views::DialogDelegateView)

 public:
  using InstallerState = bruschetta::BruschettaInstaller::State;
  using InstallerFactory =
      base::RepeatingCallback<std::unique_ptr<bruschetta::BruschettaInstaller>(
          Profile* profile,
          base::OnceClosure close_callback)>;
  using InstallResultCallback =
      base::OnceCallback<void(bruschetta::BruschettaInstallResult)>;

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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;

  // bruschetta::BruschettaInstaller::Observer implementation.
  void StateChanged(InstallerState state) override;
  void Error(bruschetta::BruschettaInstallResult error) override;

  // Public for testing purposes.
  std::u16string GetPrimaryMessage() const;
  std::u16string GetSecondaryMessage() const;
  views::Link* GetLinkLabelForTesting() const { return link_label_; }
  void OnInstallationEnded();

  // Let tests inject mock installers.
  void set_installer_factory_for_testing(InstallerFactory factory) {
    installer_factory_ = std::move(factory);
  }

  views::ProgressBar* progress_bar_for_testing() { return progress_bar_; }

  void set_finish_callback_for_testing(InstallResultCallback callback) {
    finish_callback_ = std::move(callback);
  }

 private:
  class TitleLabel;
  enum class State {
    kConfirmInstall,  // Waiting for user to start installation.
    kInstalling,      // Installation in progress.
    kCleaningUp,      // Cleaning up a partial install.
    kFailed,          // Failed to install.
    kFailedCleanup,   // Failed to install then also failed to clean up.
    // Note: No succeeded state since we close the installer upon success.
  };

  ~BruschettaInstallerView() override;

  // Returns the dialog buttons that should be displayed, based on the current
  // |state_|.
  int GetCurrentDialogButtons() const;

  // Returns the label for a dialog |button|, based on the current |state_|.
  std::u16string GetCurrentDialogButtonLabel(
      ui::mojom::DialogButton button) const;

  // views::DialogDelegateView implementation.
  void AddedToWidget() override;

  // ash::ColorModeObserver overrides.
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void SetPrimaryMessageLabel();
  void SetSecondaryMessageLabel();

  void StartInstallation();
  void OnStateUpdated();

  void CleanupPartialInstall();
  void UninstallBruschettaFinished(bool success);

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<views::Label> primary_message_label_ = nullptr;
  raw_ptr<views::Label> secondary_message_label_ = nullptr;
  raw_ptr<views::Link> link_label_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> radio_button_container_ = nullptr;

  GURL learn_more_url_;
  base::flat_map<std::string, raw_ptr<views::RadioButton, DanglingUntriaged>>
      radio_buttons_;
  std::string selected_config_;

  State state_ = State::kConfirmInstall;
  InstallerState installing_state_ = InstallerState::kInstallStarted;

  base::ScopedObservation<bruschetta::BruschettaInstaller,
                          bruschetta::BruschettaInstaller::Observer>
      observation_;

  std::unique_ptr<bruschetta::BruschettaInstaller> installer_;
  InstallerFactory installer_factory_;
  guest_os::GuestId guest_id_;
  bruschetta::BruschettaInstallResult error_ =
      bruschetta::BruschettaInstallResult::kUnknown;
  bool is_destroying_ = false;
  InstallResultCallback finish_callback_;

  base::WeakPtrFactory<BruschettaInstallerView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BRUSCHETTA_BRUSCHETTA_INSTALLER_VIEW_H_
