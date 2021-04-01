// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace apps {
class AppServiceProxyChromeOs;
}  // namespace apps

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

// Search results for the Help App (aka Explore).
// TODO(b/171519930): This is still a WIP, and needs to have results added.
class HelpAppResult : public ChromeSearchResult {
 public:
  HelpAppResult(float relevance, Profile* profile, const gfx::ImageSkia& icon);
  ~HelpAppResult() override;

  HelpAppResult(const HelpAppResult&) = delete;
  HelpAppResult& operator=(const HelpAppResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  Profile* const profile_;
};

// Provider results for Help App.
class HelpAppProvider : public SearchProvider,
                        public apps::AppRegistryCache::Observer {
 public:
  explicit HelpAppProvider(Profile* profile);
  ~HelpAppProvider() override;

  HelpAppProvider(const HelpAppProvider&) = delete;
  HelpAppProvider& operator=(const HelpAppProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void AppListShown() override;
  ash::AppListSearchResultType ResultType() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);
  void LoadIcon();

  apps::AppServiceProxyChromeOs* app_service_proxy_;
  gfx::ImageSkia icon_;
  Profile* const profile_;
  base::WeakPtrFactory<HelpAppProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
