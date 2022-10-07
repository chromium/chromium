// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

// Runs a series of icon health checks for |app_id|.
class WebAppIconDiagnostic {
 public:
  struct Result {
    bool has_empty_downloaded_icon_sizes = false;
    bool has_generated_icon_flag = false;
    // TODO(https://crbug.com/1353659): Add more checks.
  };

  WebAppIconDiagnostic(Profile* profile, AppId app_id);
  ~WebAppIconDiagnostic();

  void Run(base::OnceCallback<void(absl::optional<Result>)> result_callback);

 private:
  raw_ptr<Profile> profile_ = nullptr;
  AppId app_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_DIAGNOSTIC_H_
