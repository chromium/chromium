// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_prefs.h"

#include <algorithm>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {
namespace url_handler_prefs {
namespace {

constexpr const char* kAppId = "app_id";
constexpr const char* kExcludePaths = "exclude_paths";
constexpr const char* kHasOriginWildcard = "has_origin_wildcard";
constexpr const char* kPaths = "paths";
constexpr const char* kProfilePath = "profile_path";

// Returns true if |url| has the same origin as origin_str. If
// |look_for_subdomains| is true, url must have an origin that extends
// |origin_str| by at least one sub-domain.
bool UrlMatchesOrigin(const GURL& url,
                      const std::string& origin_str,
                      const bool look_for_subdomains) {
  url::Origin origin = url::Origin::Create(GURL(origin_str));
  url::Origin url_origin = url::Origin::Create(url);
  if (origin.scheme() != url_origin.scheme() ||
      origin.port() != url_origin.port())
    return false;

  const std::string& origin_host = origin.host();
  const std::string& url_origin_host = url_origin.host();
  if (look_for_subdomains) {
    size_t pos = url_origin_host.find(origin_host);
    if (pos == std::string::npos || pos == 0)
      return false;

    return url_origin_host.substr(pos) == origin_host;
  } else {
    return origin_host == url_origin_host;
  }
}

// Return if |url_path| matches any path in |paths|. A path in |paths| can
// contain one wildcard * at the end.
// |url_path| always starts with a '/', as it's the result of GURL::path().
bool UrlPathMatches(const std::string& url_path, const base::Value& paths) {
  if (!paths.is_list())
    return false;

  for (const auto& path : paths.GetList()) {
    std::string path_str = path.GetString();
    if (path_str.back() == '*') {
      // Remove the wildcard and check if it's the same as the first several
      // characters of |url_path|.
      path_str = path_str.substr(0, path_str.length() - 1);
      if (base::StartsWith(url_path, path_str))
        return true;
    } else {
      // |path_str| doesn't contain a wildcard, check for an exact match.
      if (path_str == url_path)
        return true;
    }
  }
  return false;
}

// Given a list of handlers that matched an origin, apply the rules in each
// handler against |url| and return only handlers that match |url| by appending
// to |matches|.
// |origin_trimmed| indicates if the input URL's origin had to be shortened to
// find a matching key. If true, filter out and matches that did not allow an
// origin prefix wildcard in their manifest.
void FilterAndAddMatches(const base::Value& all_handlers,
                         const GURL& url,
                         bool origin_trimmed,
                         std::vector<UrlHandlerLaunchParams>& matches) {
  if (!all_handlers.is_list())
    return;

  for (auto& handler : all_handlers.GetList()) {
    if (!handler.is_dict())
      continue;

    const std::string* const app_id = handler.FindStringKey(kAppId);
    if (!app_id || app_id->empty())
      continue;

    base::Optional<base::FilePath> profile_path =
        util::ValueToFilePath(handler.FindKey(kProfilePath));
    if (!profile_path || profile_path->empty())
      continue;

    if (origin_trimmed) {
      base::Optional<bool> has_wildcard =
          handler.FindBoolKey(kHasOriginWildcard);
      if (!has_wildcard || !*has_wildcard)
        continue;
    }

    const std::string& url_path = url.path();
    bool path_matches = true;
    const base::Value* const paths = handler.FindListKey(kPaths);

    bool paths_exist = paths && paths->is_list() && !paths->GetList().empty();
    if (paths_exist)
      path_matches = UrlPathMatches(url_path, *paths);

    const base::Value* const exclude_paths = handler.FindListKey(kExcludePaths);
    bool exclude_paths_exist = exclude_paths && exclude_paths->is_list() &&
                               !exclude_paths->GetList().empty();
    if (exclude_paths_exist) {
      bool match_exclude_path = UrlPathMatches(url_path, *exclude_paths);
      // If |paths| and |exclude_paths| are both not empty, only |url_path|
      // that matches a path and does not match an exclude path is considered
      // a match.
      // If |paths| is empty and |exclude_paths| is not, |url_path| that does
      // not match any exclude path is considered a match.
      path_matches = paths_exist ? (path_matches && !match_exclude_path)
                                 : !match_exclude_path;
    }

    if (path_matches)
      matches.emplace_back(*profile_path, *app_id, url);
  }
}

// Returns the URL handlers stored in |pref_value| that match |url|'s origin.
std::vector<UrlHandlerLaunchParams> FindMatches(const base::Value& pref_value,
                                                const GURL& url) {
  std::vector<UrlHandlerLaunchParams> matches;

  if (!pref_value.is_dict())
    return matches;

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque())
    return matches;

  if (origin.scheme() != url::kHttpsScheme)
    return matches;

  std::string origin_str = origin.Serialize();
  bool origin_trimmed(false);

  while (true) {
    const base::Value* const all_handlers = pref_value.FindListKey(origin_str);
    if (all_handlers) {
      DCHECK(UrlMatchesOrigin(url, origin_str, origin_trimmed));
      FilterAndAddMatches(*all_handlers, url, origin_trimmed, matches);
    }

    // If a key matching the input URL's origin is not found, shorten the origin
    // by sub-domain and try again. This enables matching against manifest
    // "url_handlers" origins that contain wildcard prefixes.
    auto found = origin_str.find('.');
    if (found != std::string::npos) {
      // Trim origin to after next '.' character if there is one.
      origin_str = base::StrCat({"https://", origin_str.substr(found + 1)});
      origin_trimmed = true;
    } else {
      // There is no more '.'. Stop looking.
      break;
    }
  }
  return matches;
}

base::Value GetPathsValue(const std::vector<std::string>& paths) {
  base::Value paths_value(base::Value::Type::LIST);
  for (const auto& path : paths)
    paths_value.Append(path);

  return paths_value;
}

base::Value NewHandler(const AppId& app_id,
                       const base::FilePath& profile_path,
                       const apps::UrlHandlerInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kAppId, app_id);
  value.SetKey(kProfilePath, util::FilePathToValue(profile_path));
  value.SetBoolKey(kHasOriginWildcard, info.has_origin_wildcard);

  // Set paths and exclude paths from associated app.
  value.SetKey(kPaths, GetPathsValue(info.paths));
  value.SetKey(kExcludePaths, GetPathsValue(info.exclude_paths));

  // TODO(crbug/1072058): Set "user_permission" field when implementing user
  // settings in the chrome://settings page.
  return value;
}

// If |match_app_id| is true, returns true if |handler| has dict. values equal
// to |app_id| and |profile_path|. If |match_app_id| is false, only compare
// |profile_path|.
bool IsHandlerForApp(const AppId& app_id,
                     const base::FilePath& profile_path,
                     bool match_app_id,
                     const base::Value& handler) {
  const std::string* const app_id_local = handler.FindStringKey(kAppId);
  base::Optional<base::FilePath> profile_path_local =
      util::ValueToFilePath(handler.FindKey(kProfilePath));

  if (!app_id_local || !profile_path_local)
    return false;

  if (*profile_path_local != profile_path)
    return false;

  return !match_app_id || *app_id_local == app_id;
}

// Removes entries that match |profile_path| and |app_id|.
// |profile_path| is always compared while |app_id| is only compared when it is
// not empty.
void RemoveEntries(base::Value& pref_value,
                   const AppId& app_id,
                   const base::FilePath& profile_path) {
  if (!pref_value.is_dict())
    return;

  std::vector<std::string> origins_to_remove;
  for (auto origin_value : pref_value.DictItems()) {
    base::Value::ListStorage handlers = origin_value.second.TakeList();
    handlers.erase(
        std::remove_if(handlers.begin(), handlers.end(),
                       [&app_id, &profile_path](const base::Value& handler) {
                         return IsHandlerForApp(
                             app_id, profile_path,
                             /*match_app_id=*/!app_id.empty(), handler);
                       }),
        handlers.end());
    // Replace list if any entries remain.
    if (!handlers.empty()) {
      origin_value.second = base::Value(std::move(handlers));
    } else {
      origins_to_remove.push_back(origin_value.first);
    }
  }

  for (const auto& origin_to_remove : origins_to_remove)
    pref_value.RemoveKey(origin_to_remove);
}
}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterDictionaryPref(prefs::kWebAppsUrlHandlerInfo);
}

void AddWebApp(PrefService* local_state,
               const AppId& app_id,
               const base::FilePath& profile_path,
               const apps::UrlHandlers& url_handlers) {
  if (profile_path.empty() || url_handlers.empty())
    return;

  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  for (const apps::UrlHandlerInfo& handler_info : url_handlers) {
    const url::Origin& origin = handler_info.origin;
    if (origin.opaque())
      continue;

    base::Value new_handler(NewHandler(app_id, profile_path, handler_info));
    base::Value* const handlers_mutable =
        pref_value->FindListKey(origin.Serialize());
    // One or more apps are already associated with this origin.
    if (handlers_mutable) {
      base::Value::ListStorage handlers = handlers_mutable->TakeList();
      auto it =
          std::find_if(handlers.begin(), handlers.end(),
                       [&app_id, &profile_path](const base::Value& handler) {
                         return IsHandlerForApp(app_id, profile_path,
                                                /*match_app_id=*/true, handler);
                       });
      // If there is already an entry with the same app_id and profile, replace
      // it. Otherwise, add new entry to the end.
      if (it != handlers.end()) {
        *it = std::move(new_handler);
      } else {
        handlers.push_back(std::move(new_handler));
      }
      *handlers_mutable = base::Value(std::move(handlers));
    } else {
      base::Value new_handlers(base::Value::Type::LIST);
      new_handlers.Append(std::move(new_handler));
      pref_value->SetKey(origin.Serialize(), std::move(new_handlers));
    }
  }
}

void UpdateWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path,
                  const apps::UrlHandlers& url_handlers) {
  // TODO(crbug/1072058): Handle "user_permission" field when it is
  // implemented.
  RemoveWebApp(local_state, app_id, profile_path);
  AddWebApp(local_state, app_id, profile_path, url_handlers);
}

void RemoveWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path) {
  if (app_id.empty() || profile_path.empty())
    return;

  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  RemoveEntries(*pref_value, app_id, profile_path);
}

void RemoveProfile(PrefService* local_state,
                   const base::FilePath& profile_path) {
  if (profile_path.empty())
    return;

  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  RemoveEntries(*pref_value, /*app_id*/ "", profile_path);
}

void Clear(PrefService* local_state) {
  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  pref_value->DictClear();
}

std::vector<UrlHandlerLaunchParams> FindMatchingUrlHandlers(
    PrefService* local_state,
    const GURL& url) {
  if (!url.is_valid())
    return {};

  const base::Value* const pref_value =
      local_state->Get(prefs::kWebAppsUrlHandlerInfo);
  if (!pref_value || !pref_value->is_dict())
    return {};

  return FindMatches(*pref_value, url);
}

}  // namespace url_handler_prefs
}  // namespace web_app
