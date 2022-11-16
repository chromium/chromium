// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class WebAppProvider;
class WebApp;

// Runs a series of icon health checks for |app_id|.
class WebAppIconDiagnostic {
 public:
  struct Result {
    bool has_empty_downloaded_icon_sizes = false;
    bool has_generated_icon_flag = false;
    bool has_generated_icon_flag_false_negative = false;
    bool has_generated_icon_bitmap = false;
    bool has_empty_icon_bitmap = false;
    bool has_empty_icon_file = false;
    bool has_missing_icon_file = false;
    // TODO(https://crbug.com/1353659): Add more checks.

    // Keep attributes in sync with operator<< and IsAnyFallbackUsed.
    bool IsAnyFallbackUsed() const {
      return has_empty_downloaded_icon_sizes || has_generated_icon_flag ||
             has_generated_icon_flag_false_negative ||
             has_generated_icon_bitmap || has_empty_icon_bitmap ||
             has_empty_icon_file || has_missing_icon_file;
    }
  };

  WebAppIconDiagnostic(Profile* profile, AppId app_id);
  ~WebAppIconDiagnostic();

  void Run(base::OnceCallback<void(absl::optional<Result>)> result_callback);

 private:
  base::WeakPtr<WebAppIconDiagnostic> GetWeakPtr();

  void CallResultCallback();

  void LoadIconFromProvider(
      WebAppIconManager::ReadIconWithPurposeCallback callback);
  void DiagnoseGeneratedOrEmptyIconBitmap(base::OnceClosure done_callback,
                                          IconPurpose purpose,
                                          SkBitmap icon_bitmap);

  void CheckForEmptyOrMissingIconFiles(
      base::OnceCallback<void(WebAppIconManager::IconFilesCheck)>
          icon_files_callback);
  void DiagnoseEmptyOrMissingIconFiles(
      base::OnceClosure done_callback,
      WebAppIconManager::IconFilesCheck icon_files_check);

  const raw_ptr<Profile> profile_;
  const AppId app_id_;

  const raw_ptr<WebAppProvider> provider_;
  const raw_ptr<const WebApp> app_;

  absl::optional<SquareSizePx> icon_size_;

  absl::optional<Result> result_;
  base::OnceCallback<void(absl::optional<Result>)> result_callback_;

  base::WeakPtrFactory<WebAppIconDiagnostic> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         const WebAppIconDiagnostic::Result result);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_
