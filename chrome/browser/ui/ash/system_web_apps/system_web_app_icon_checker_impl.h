// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_icon_checker.h"
#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_diagnostic.h"

namespace ash {

// This class calls into web app subsystems to check if system web app icons are
// healthy.
class SystemWebAppIconCheckerImpl : public SystemWebAppIconChecker {
 public:
  explicit SystemWebAppIconCheckerImpl(Profile* profile);
  ~SystemWebAppIconCheckerImpl() override;

  // SystemWebAppIconChecker:
  void StartCheck(const std::vector<web_app::AppId>& app_ids,
                  base::OnceCallback<void(IconState)> callback) override;
  void StopCheck() override;

 private:
  void OnChecksDone(
      base::OnceCallback<void(IconState)> callback,
      std::vector<absl::optional<web_app::WebAppIconDiagnostic::Result>>
          results);

  raw_ptr<Profile> profile_;
  std::vector<std::unique_ptr<web_app::WebAppIconDiagnostic>> checkers_;
  base::WeakPtrFactory<SystemWebAppIconCheckerImpl> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_IMPL_H_
