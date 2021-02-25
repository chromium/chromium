// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"

#include <vector>

#include "base/feature_list.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/url_handler_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_constants.h"

namespace web_app {

UrlHandlerManagerImpl::UrlHandlerManagerImpl(Profile* profile)
    : UrlHandlerManager(profile) {}

UrlHandlerManagerImpl::~UrlHandlerManagerImpl() = default;

// static
std::vector<web_app::UrlHandlerLaunchParams>
UrlHandlerManagerImpl::GetUrlHandlerMatches(
    PrefService* local_state,
    const base::CommandLine& command_line) {
  std::vector<web_app::UrlHandlerLaunchParams> results;

  if (!base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers))
    return results;

  // Return early to not interfere with switch based app launches.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return results;
  }

  std::vector<GURL> urls;
  for (const auto& arg : command_line.GetArgs()) {
    GURL potential_url(arg);
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

  // TODO(crbug/1072058): Refactor UrlHandlerPrefs to provide functions instead
  // of being a class that can be instantiated.
  UrlHandlerPrefs url_handler_prefs(local_state);
  base::Optional<std::vector<UrlHandlerPrefs::Match>> prefs_matches =
      url_handler_prefs.FindMatchingUrlHandlers(urls.front());
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

bool UrlHandlerManagerImpl::RegisterUrlHandlers(const AppId& app_id) {
  if (!base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers))
    return false;

  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);
  if (url_handlers.empty())
    return true;

  UrlHandlerPrefs url_handler_prefs(GetLocalState());
  url_handler_prefs.AddWebApp(app_id, profile()->GetPath(), url_handlers);
  return true;
}

bool UrlHandlerManagerImpl::UnregisterUrlHandlers(const AppId& app_id) {
  UrlHandlerPrefs url_handler_prefs(GetLocalState());
  url_handler_prefs.RemoveWebApp(app_id, profile()->GetPath());
  return true;
}

bool UrlHandlerManagerImpl::UpdateUrlHandlers(const AppId& app_id) {
  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);
  UrlHandlerPrefs url_handler_prefs(GetLocalState());

  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableUrlHandlers)) {
    url_handler_prefs.RemoveWebApp(app_id, profile()->GetPath());
    return false;
  } else {
    url_handler_prefs.UpdateWebApp(app_id, profile()->GetPath(), url_handlers);
    return true;
  }
}

void UrlHandlerManagerImpl::SetLocalStateForTesting(PrefService* local_state) {
  override_pref_service_ = local_state;
}

PrefService* UrlHandlerManagerImpl::GetLocalState() {
  if (override_pref_service_)
    return override_pref_service_;

  return g_browser_process->local_state();
}

}  // namespace web_app
