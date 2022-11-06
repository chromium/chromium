// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_

#include <vector>

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::office_fallback {

using DialogChoiceCallback =
    base::OnceCallback<void(const std::string& choice)>;

// The reason for why the user's file can't open
enum class FallbackReason {
  kOffline,
  kDriveUnavailable,
  kOneDriveUnavailable,
  kErrorOpeningWeb,
};

// Defines the web dialog used to allow users to choose what to do when failing
// to open office files.
class OfficeFallbackDialog : public SystemWebDialogDelegate {
 public:
  OfficeFallbackDialog(const OfficeFallbackDialog&) = delete;
  OfficeFallbackDialog& operator=(const OfficeFallbackDialog&) = delete;

  // Creates and shows the dialog. Returns true if a new dialog has been
  // effectively created.
  static bool Show(const std::vector<storage::FileSystemURL>& file_urls,
                   const FallbackReason fallback_reason,
                   const std::u16string& task_title,
                   DialogChoiceCallback callback);

  // Receives user's fallback choice and runs callback. Does nothing
  // if they chose `cancel`.
  void OnDialogClosed(const std::string& json_retval) override;

  ~OfficeFallbackDialog() override;

 protected:
  OfficeFallbackDialog(const std::vector<storage::FileSystemURL>& file_urls,
                       const FallbackReason fallback_reason,
                       const std::u16string& task_title,
                       DialogChoiceCallback callback);
  std::string GetDialogArgs() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;

 private:
  const std::vector<storage::FileSystemURL> file_urls_;
  const FallbackReason fallback_reason_;
  const std::u16string task_title_;
  DialogChoiceCallback callback_;
};

}  // namespace ash::office_fallback

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_DIALOG_H_
