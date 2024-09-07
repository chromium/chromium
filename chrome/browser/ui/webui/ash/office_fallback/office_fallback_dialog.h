// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_

#include <vector>

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::office_fallback {

// The reason why the user's file can't open.
enum class FallbackReason {
  kOffline,
  kDriveDisabled,
  kNoDriveService,
  kDriveAuthenticationNotReady,
  kDriveFsInterfaceError,
  kMeteredConnection,
  kDisableDrivePreferenceSet,
  kDriveDisabledForAccountType,
  kWaitingForUpload,
  kAndroidOneDriveUnsupportedLocation,
};

using DialogChoiceCallback =
    base::OnceCallback<void(std::optional<const std::string>)>;

// Defines the web dialog used to allow users to choose what to do when failing
// to open office files.
class OfficeFallbackDialog : public SystemWebDialogDelegate {
 public:
  OfficeFallbackDialog(const OfficeFallbackDialog&) = delete;
  OfficeFallbackDialog& operator=(const OfficeFallbackDialog&) = delete;

  // Creates and shows the dialog. Returns true if a new dialog has been
  // effectively created.
  static bool Show(const std::vector<storage::FileSystemURL>& file_urls,
                   FallbackReason fallback_reason,
                   const std::string& task_title,
                   DialogChoiceCallback callback);

  // Receives user's dialog choice and runs callback.
  void OnDialogClosed(const std::string& choice) override;

  ~OfficeFallbackDialog() override;

 protected:
  OfficeFallbackDialog(const std::vector<storage::FileSystemURL>& file_urls,
                       const std::string& title_text,
                       const std::string& reason_message,
                       const std::string& instructions_message,
                       const bool& enable_retry_option,
                       const bool& enable_quick_office_option,
                       const int& width,
                       const int& height,
                       DialogChoiceCallback callback);
  std::string GetDialogArgs() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowCloseButton() const override;

 private:
  const std::vector<storage::FileSystemURL> file_urls_;
  const std::string title_text_;
  const std::string reason_message_;
  const std::string instructions_message_;
  const bool enable_retry_option_;
  const bool enable_quick_office_option_;
  const int width_;
  const int height_;
  DialogChoiceCallback callback_;
};

}  // namespace ash::office_fallback

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_
