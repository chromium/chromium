// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ui/views/borealis/borealis_installer_error_dialog.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class BorealisBetaBadge;
class BoxLayout;
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

class Profile;

namespace borealis {

void ShowBorealisInstallerView(Profile* profile);

}  // namespace borealis

// The front end for the Borealis installation process, works closely with
// "chrome/browser/ash/borealis/borealis_installer.h".
class BorealisInstallerView : public views::DialogDelegateView,
                              public borealis::BorealisInstaller::Observer,
                              public ash::ColorModeObserver {
 public:
  METADATA_HEADER(BorealisInstallerView);

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

  // borealis::BorealisInstaller::Observer implementation.
  void OnStateUpdated(
      borealis::BorealisInstaller::InstallingState new_state) override;
  void OnProgressUpdated(double fraction_complete) override;
  void OnInstallationEnded(borealis::mojom::InstallResult result,
                           const std::string& error_description) override;
  void OnCancelInitiated() override {}

  // Public for testing purposes.
  std::u16string GetPrimaryMessage() const;
  std::u16string GetSecondaryMessage() const;

  void SetInstallingStateForTesting(InstallingState new_state);

 private:
  class TitleLabel;
  enum class State {
    kConfirmInstall,  // Waiting for user to start installation.
    kInstalling,      // Installation in progress.
    kCompleted,       // Installation process completed.
  };

  ~BorealisInstallerView() override;

  // Returns the dialog buttons that should be displayed, based on the current
  // |state_| and error |reason_| (if relevant).
  int GetCurrentDialogButtons() const;

  // Returns the label for a dialog |button|, based on the current |state_|
  // and error |reason_| (if relevant).
  std::u16string GetCurrentDialogButtonLabel(ui::DialogButton button) const;

  // Called by the error dialog when the user closes it.
  void OnErrorDialogDismissed(views::borealis::ErrorDialogChoice choice);

  void OnStateUpdated();

  // views::DialogDelegateView implementation.
  void AddedToWidget() override;

  // ash::ColorModeObserver overrides.
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void SetPrimaryMessageLabel();
  void SetSecondaryMessageLabel();
  void SetProgressMessageLabel();
  void SetImage();

  void StartInstallation();

  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> primary_message_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> secondary_message_label_ = nullptr;
  raw_ptr<views::BorealisBetaBadge, ExperimentalAsh> beta_badge_ = nullptr;
  raw_ptr<views::ProgressBar, ExperimentalAsh> progress_bar_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh>
      installation_progress_percentage_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> installation_progress_separator_ =
      nullptr;
  raw_ptr<views::Label, ExperimentalAsh> installation_progress_eta_label_ =
      nullptr;
  raw_ptr<views::BoxLayout, ExperimentalAsh> right_container_layout_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> big_image_ = nullptr;

  base::Time install_start_time_;
  State state_ = State::kConfirmInstall;
  InstallingState installing_state_ = InstallingState::kInactive;
  absl::optional<borealis::mojom::InstallResult> result_;

  base::ScopedObservation<borealis::BorealisInstaller,
                          borealis::BorealisInstaller::Observer>
      observation_;

  base::WeakPtrFactory<BorealisInstallerView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_VIEW_H_
