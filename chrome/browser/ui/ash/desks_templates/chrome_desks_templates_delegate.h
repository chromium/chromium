// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_CHROME_DESKS_TEMPLATES_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_CHROME_DESKS_TEMPLATES_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/desks_templates_delegate.h"
#include "base/callback_forward.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace base {
class CancelableTaskTracker;
}  // namespace base

class ChromeDesksTemplatesDelegate : public ash::DesksTemplatesDelegate {
 public:
  ChromeDesksTemplatesDelegate();
  ChromeDesksTemplatesDelegate(const ChromeDesksTemplatesDelegate&) = delete;
  ChromeDesksTemplatesDelegate& operator=(const ChromeDesksTemplatesDelegate&) =
      delete;
  ~ChromeDesksTemplatesDelegate() override;

  // ash::DesksTemplatesDelegate:
  std::unique_ptr<app_restore::AppLaunchInfo> GetAppLaunchDataForDeskTemplate(
      aura::Window* window) const override;
  desks_storage::DeskModel* GetDeskModel() override;
  bool IsIncognitoWindow(aura::Window* window) const override;
  absl::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& identifier,
      const ui::ColorProvider* color_provider) const override;
  void GetFaviconForUrl(const std::string& page_url,
                        int desired_icon_size,
                        favicon_base::FaviconRawBitmapCallback callback,
                        base::CancelableTaskTracker* tracker) const override;
  void GetIconForAppId(const std::string& app_id,
                       int desired_icon_size,
                       base::OnceCallback<void(apps::IconValuePtr icon_value)>
                           callback) const override;
  void LaunchAppsFromTemplate(
      std::unique_ptr<ash::DeskTemplate> desk_template) override;
  bool IsWindowSupportedForDeskTemplate(aura::Window* window) const override;
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_CHROME_DESKS_TEMPLATES_DELEGATE_H_
