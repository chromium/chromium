// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/url_handler_prefs.h"
#include "chrome/browser/web_applications/components/web_app_origin_association_manager.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_constants.h"

namespace web_app {

UrlHandlerManagerImpl::UrlHandlerManagerImpl(Profile* profile)
    : UrlHandlerManager(profile),
      association_manager_(std::make_unique<WebAppOriginAssociationManager>()) {
}

UrlHandlerManagerImpl::~UrlHandlerManagerImpl() = default;

// static
std::vector<UrlHandlerLaunchParams> UrlHandlerManagerImpl::GetUrlHandlerMatches(
    const base::CommandLine& command_line) {
  std::vector<UrlHandlerLaunchParams> results;

  if (!base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers))
    return results;

  // Return early to not interfere with switch based app launches.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return results;
  }

  std::vector<GURL> urls;
  for (const auto& arg : command_line.GetArgs()) {
#if defined(OS_WIN)
    GURL potential_url(base::WideToUTF16(arg));
#else
    GURL potential_url(arg);
#endif
    if (potential_url.is_valid() && potential_url.IsStandard())
      urls.push_back(potential_url);
  }
  // Only handle commandline with single URL. If multiple URLs are found, return
  // early so they can be handled normally. If the OS calls the system default
  // browser to handle a URL activation, this is usually with a single URL.
  if (urls.empty() || urls.size() > 1)
    return results;

  if (!urls.front().SchemeIs(url::kHttpsScheme))
    return results;

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return results;

  base::Optional<std::vector<url_handler_prefs::Match>> prefs_matches =
      url_handler_prefs::FindMatchingUrlHandlers(local_state, urls.front());
  if (!prefs_matches || prefs_matches->empty())
    return results;

  for (const auto& prefs_match : *prefs_matches) {
    const auto& target_app_id = prefs_match.app_id;
    const auto& target_profile_path = prefs_match.profile_path;
    if (target_app_id.empty())
      continue;

    results.emplace_back(target_profile_path, target_app_id, urls.front());
  }
  return results;
}

void UrlHandlerManagerImpl::RegisterUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableUrlHandlers)) {
    std::move(callback).Run(false);
    return;
  }

  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);
  // TODO(crbug/1072058): Only get associations for user-enabled url handlers.
  if (url_handlers.empty()) {
    std::move(callback).Run(true);
    return;
  }

  association_manager_->GetWebAppOriginAssociations(
      registrar()->GetAppManifestUrl(app_id), std::move(url_handlers),
      base::BindOnce(&UrlHandlerManagerImpl::OnDidGetAssociationsAtInstall,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void UrlHandlerManagerImpl::OnDidGetAssociationsAtInstall(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback,
    apps::UrlHandlers url_handlers) {
  if (!url_handlers.empty()) {
    url_handler_prefs::AddWebApp(GetLocalState(), app_id, profile()->GetPath(),
                                 std::move(url_handlers));
  }
  std::move(callback).Run(true);
}

bool UrlHandlerManagerImpl::UnregisterUrlHandlers(const AppId& app_id) {
  url_handler_prefs::RemoveWebApp(GetLocalState(), app_id,
                                  profile()->GetPath());
  return true;
}

void UrlHandlerManagerImpl::UpdateUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);

  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableUrlHandlers)) {
    url_handler_prefs::RemoveWebApp(GetLocalState(), app_id,
                                    profile()->GetPath());
    std::move(callback).Run(false);
    return;
  }

  association_manager_->GetWebAppOriginAssociations(
      registrar()->GetAppManifestUrl(app_id), std::move(url_handlers),
      base::BindOnce(&UrlHandlerManagerImpl::OnDidGetAssociationsAtUpdate,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void UrlHandlerManagerImpl::OnDidGetAssociationsAtUpdate(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback,
    apps::UrlHandlers url_handlers) {
  // TODO(crbug/1072058): Only overwrite existing url_handlers if associations
  // changed. Allow this after user permission is implemented.
  url_handler_prefs::UpdateWebApp(GetLocalState(), app_id, profile()->GetPath(),
                                  std::move(url_handlers));
  std::move(callback).Run(true);
}

void UrlHandlerManagerImpl::SetAssociationManagerForTesting(
    std::unique_ptr<WebAppOriginAssociationManager> manager) {
  association_manager_ = std::move(manager);
}

PrefService* UrlHandlerManagerImpl::GetLocalState() {
  return g_browser_process->local_state();
}

}  // namespace web_app
