// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
class Link;
class ProgressBar;
}  // namespace views

class Profile;

// The front end for Plugin VM, shown the first time the user launches it.
class PluginVmInstallerView : public views::BubbleDialogDelegateView,
                              public plugin_vm::PluginVmInstaller::Observer {
  METADATA_HEADER(PluginVmInstallerView, views::BubbleDialogDelegateView)

 public:
  explicit PluginVmInstallerView(Profile* profile);

  PluginVmInstallerView(const PluginVmInstallerView&) = delete;
  PluginVmInstallerView& operator=(const PluginVmInstallerView&) = delete;

  static PluginVmInstallerView* GetActiveViewForTesting();

  // views::BubbleDialogDelegateView implementation.
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // plugin_vm::PluginVmImageDownload::Observer implementation.
  void OnStateUpdated(
      plugin_vm::PluginVmInstaller::InstallingState new_state) override;
  void OnProgressUpdated(double fraction_complete) override;
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length) override;
  void OnVmExists() override;
  void OnCreated() override;
  void OnImported() override;
  void OnError(plugin_vm::PluginVmInstaller::FailureReason reason) override;
  void OnCancelFinished() override;

  // Public for testing purposes.
  std::u16string GetTitle() const;
  std::u16string GetMessage() const;
  views::Label* GetTitleViewForTesting() { return title_label_; }
  views::Label* GetMessageViewForTesting() { return message_label_; }
  views::Label* GetDownloadProgressMessageViewForTesting() {
    return download_progress_message_label_;
  }

  void SetFinishedCallbackForTesting(
      base::OnceCallback<void(bool success)> callback);

 private:
  enum class State {
    kConfirmInstall,  // Waiting for user to start installation.
    kInstalling,      // Installation in progress.
    kCreated,         // A brand new VM has been created using ISO image.
    kImported,        // Downloaded VM image has been imported successfully.
    kError,           // Something unexpected happened.
  };

  using InstallingState = plugin_vm::PluginVmInstaller::InstallingState;

  ~PluginVmInstallerView() override;

  int GetCurrentDialogButtons() const;
  std::u16string GetCurrentDialogButtonLabel(
      ui::mojom::DialogButton button) const;

  void OnStateUpdated();
  void OnLinkClicked();
  // views::BubbleDialogDelegateView implementation.
  void AddedToWidget() override;
  void OnThemeChanged() override;

  std::u16string GetDownloadProgressMessage(uint64_t bytes_downloaded,
                                            int64_t content_length) const;
  void SetTitleLabel();
  void SetMessageLabel();
  void SetBigImage();

  void StartInstallation();

  raw_ptr<Profile> profile_ = nullptr;
  std::u16string app_name_;
  raw_ptr<plugin_vm::PluginVmInstaller> plugin_vm_installer_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> message_label_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::Label> download_progress_message_label_ = nullptr;
  raw_ptr<views::BoxLayout> lower_container_layout_ = nullptr;
  raw_ptr<views::ImageView> big_image_ = nullptr;
  raw_ptr<views::Link, DanglingUntriaged> learn_more_link_ = nullptr;

  State state_ = State::kConfirmInstall;
  InstallingState installing_state_ = InstallingState::kInactive;
  std::optional<plugin_vm::PluginVmInstaller::FailureReason> reason_;

  base::OnceCallback<void(bool success)> finished_callback_for_testing_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PLUGIN_VM_PLUGIN_VM_INSTALLER_VIEW_H_
