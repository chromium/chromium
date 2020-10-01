// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

class WebApp;

using Registry = std::map<AppId, std::unique_ptr<WebApp>>;

// A registry model. This is a read-only container, which owns WebApp objects.
class WebAppRegistrar : public AppRegistrar, public ProfileManagerObserver {
 public:
  explicit WebAppRegistrar(Profile* profile);
  ~WebAppRegistrar() override;

  bool is_empty() const { return registry_.empty(); }

  const WebApp* GetAppById(const AppId& app_id) const;

  // AppRegistrar:
  void Start() override;
  void Shutdown() override;
  bool IsInstalled(const AppId& app_id) const override;
  bool IsLocallyInstalled(const AppId& app_id) const override;
  bool WasInstalledByUser(const AppId& app_id) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const AppId& app_id) const override;
  std::string GetAppDescription(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppBackgroundColor(
      const AppId& app_id) const override;
  const GURL& GetAppStartUrl(const AppId& app_id) const override;
  const std::string* GetAppLaunchQueryParams(
      const AppId& app_id) const override;
  const apps::ShareTarget* GetAppShareTarget(
      const AppId& app_id) const override;
  base::Optional<GURL> GetAppScopeInternal(const AppId& app_id) const override;
  DisplayMode GetAppDisplayMode(const AppId& app_id) const override;
  DisplayMode GetAppUserDisplayMode(const AppId& app_id) const override;
  std::vector<DisplayMode> GetAppDisplayModeOverride(
      const AppId& app_id) const override;
  base::Time GetAppLastLaunchTime(const web_app::AppId& app_id) const override;
  base::Time GetAppInstallTime(const web_app::AppId& app_id) const override;
  std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const AppId& app_id) const override;
  SortedSizesPx GetAppDownloadedIconSizesAny(
      const AppId& app_id) const override;
  std::vector<WebApplicationShortcutsMenuItemInfo> GetAppShortcutsMenuItemInfos(
      const AppId& app_id) const override;
  std::vector<std::vector<SquareSizePx>>
  GetAppDownloadedShortcutsMenuIconsSizes(const AppId& app_id) const override;
  RunOnOsLoginMode GetAppRunOnOsLoginMode(const AppId& app_id) const override;
  std::vector<AppId> GetAppIds() const override;
  WebAppRegistrar* AsWebAppRegistrar() override;

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;

  // Only range-based |for| loop supported. Don't use AppSet directly.
  // Doesn't support registration and unregistration of WebApp while iterating.
  class AppSet {
   public:
    // An iterator class that can be used to access the list of apps.
    template <typename WebAppType>
    class Iter {
     public:
      using InternalIter = Registry::const_iterator;

      explicit Iter(InternalIter&& internal_iter)
          : internal_iter_(std::move(internal_iter)) {}
      Iter(Iter&&) = default;
      ~Iter() = default;

      void operator++() { ++internal_iter_; }
      WebAppType& operator*() const { return *internal_iter_->second.get(); }
      bool operator!=(const Iter& iter) const {
        return internal_iter_ != iter.internal_iter_;
      }

     private:
      InternalIter internal_iter_;
      DISALLOW_COPY_AND_ASSIGN(Iter);
    };

    explicit AppSet(const WebAppRegistrar* registrar);
    AppSet(AppSet&&) = default;
    ~AppSet();

    using iterator = Iter<WebApp>;
    using const_iterator = Iter<const WebApp>;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

   private:
    const WebAppRegistrar* const registrar_;
#if DCHECK_IS_ON()
    const size_t mutations_count_;
#endif
    DISALLOW_COPY_AND_ASSIGN(AppSet);
  };

  const AppSet AllApps() const;

 protected:
  Registry& registry() { return registry_; }
  void SetRegistry(Registry&& registry);

  void CountMutation();

 private:
  Registry registry_;
  bool registry_profile_being_deleted_ = false;
#if DCHECK_IS_ON()
  size_t mutations_count_ = 0;
#endif
  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

// A writable API for the registry model. Mutable WebAppRegistrar must be used
// only by WebAppSyncBridge.
class WebAppRegistrarMutable : public WebAppRegistrar {
 public:
  explicit WebAppRegistrarMutable(Profile* profile);
  ~WebAppRegistrarMutable() override;

  void InitRegistry(Registry&& registry);

  WebApp* GetAppByIdMutable(const AppId& app_id);
  AppSet AllAppsMutable();

  using WebAppRegistrar::CountMutation;
  using WebAppRegistrar::registry;
};

// For testing and debug purposes.
bool IsRegistryEqual(const Registry& registry, const Registry& registry2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
