// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_CHROME_SAVED_DESK_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_DESKS_CHROME_SAVED_DESK_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/saved_desk_delegate.h"
#include "base/functional/callback_forward.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class CancelableTaskTracker;
}  // namespace base

namespace desks_storage {
class DeskModel;
}  // namespace desks_storage

namespace gfx {
class ImageSkia;
}  // namespace gfx

class ChromeSavedDeskDelegate : public ash::SavedDeskDelegate {
 public:
  ChromeSavedDeskDelegate();
  ChromeSavedDeskDelegate(const ChromeSavedDeskDelegate&) = delete;
  ChromeSavedDeskDelegate& operator=(const ChromeSavedDeskDelegate&) = delete;
  ~ChromeSavedDeskDelegate() override;

  // ash::SavedDeskDelegate:
  void GetAppLaunchDataForSavedDesk(
      aura::Window* window,
      GetAppLaunchDataCallback callback) const override;
  desks_storage::DeskModel* GetDeskModel() override;
  desks_storage::AdminTemplateService* GetAdminTemplateService() override;
  bool IsWindowPersistable(aura::Window* window) const override;
  std::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& identifier,
      const ui::ColorProvider* color_provider) const override;
  void GetFaviconForUrl(
      const std::string& page_url,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      base::CancelableTaskTracker* tracker) const override;
  void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) const override;
  void LaunchAppsFromSavedDesk(
      std::unique_ptr<ash::DeskTemplate> saved_desk) override;
  bool IsWindowSupportedForSavedDesk(aura::Window* window) const override;
  std::string GetAppShortName(const std::string& app_id) override;
  bool IsAppAvailable(const std::string& app_id) const override;
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_CHROME_SAVED_DESK_DELEGATE_H_
