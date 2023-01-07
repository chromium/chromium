// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/url_handler_manager_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/url_handler_prefs.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util_win.h"
#endif

namespace web_app {

UrlHandlerManagerImpl::UrlHandlerManagerImpl(Profile* profile)
    : UrlHandlerManager(profile) {}

UrlHandlerManagerImpl::~UrlHandlerManagerImpl() = default;

// static
absl::optional<GURL> UrlHandlerManagerImpl::GetUrlFromCommandLine(
    const base::CommandLine& command_line) {
  // Return early to not interfere with switch based app launches.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return absl::nullopt;
  }

  // Only handle commandline with single URL. If multiple URLs are found, return
  // null. If the OS calls the system default browser to handle a URL
  // activation, this is usually with a single URL.
  if (command_line.GetArgs().size() != 1)
    return absl::nullopt;

#if BUILDFLAG(IS_WIN)
  GURL url(base::AsStringPiece16(command_line.GetArgs().front()));
#else
  GURL url(command_line.GetArgs().front());
#endif

  return url;
}

// static
std::vector<UrlHandlerLaunchParams> UrlHandlerManagerImpl::GetUrlHandlerMatches(
    const base::CommandLine& command_line) {
  absl::optional<GURL> url = GetUrlFromCommandLine(command_line);
  if (!url)
    return {};

  return GetUrlHandlerMatches(url.value());
}

// static
std::vector<UrlHandlerLaunchParams> UrlHandlerManagerImpl::GetUrlHandlerMatches(
    const GURL& url) {
  if (!url.is_valid() || !url.IsStandard() || !url.SchemeIs(url::kHttpsScheme))
    return {};

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return {};

  return url_handler_prefs::FindMatchingUrlHandlers(local_state, url);
}

void UrlHandlerManagerImpl::RegisterUrlHandlers(const AppId& app_id,
                                                ResultCallback callback) {
  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);
  // TODO(crbug/1072058): Only get associations for user-enabled url handlers.
  if (url_handlers.empty()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  association_manager().GetWebAppOriginAssociations(
      registrar()->GetAppManifestUrl(app_id), std::move(url_handlers),
      base::BindOnce(&UrlHandlerManagerImpl::OnDidGetAssociationsAtInstall,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void UrlHandlerManagerImpl::OnDidGetAssociationsAtInstall(
    const AppId& app_id,
    ResultCallback callback,
    apps::UrlHandlers url_handlers) {
  if (!url_handlers.empty()) {
    url_handler_prefs::AddWebApp(g_browser_process->local_state(), app_id,
                                 profile()->GetPath(), std::move(url_handlers));
  }
  std::move(callback).Run(Result::kOk);
}

bool UrlHandlerManagerImpl::UnregisterUrlHandlers(const AppId& app_id) {
  url_handler_prefs::RemoveWebApp(g_browser_process->local_state(), app_id,
                                  profile()->GetPath());
  return true;
}

void UrlHandlerManagerImpl::UpdateUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  auto url_handlers = registrar()->GetAppUrlHandlers(app_id);
  association_manager().GetWebAppOriginAssociations(
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
  url_handler_prefs::UpdateWebApp(g_browser_process->local_state(), app_id,
                                  profile()->GetPath(),
                                  std::move(url_handlers));
  std::move(callback).Run(true);
}

}  // namespace web_app
