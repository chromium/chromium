// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_prefs.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace web_app {
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

// Given a list of handlers that matched an origin, apply the rules in each
// handler against |url| and return only handlers that match |url|.
// |origin_trimmed| indicates if the input URL's origin had to be shortened to
// find a matching key. If true, filter out and matches that did not allow an
// origin prefix wildcard in their manifest.
base::Optional<std::vector<UrlHandlerPrefs::Match>> FilterMatches(
    const base::Value& all_handlers,
    const GURL& url,
    bool origin_trimmed) {
  if (!all_handlers.is_list())
    return base::nullopt;

  std::vector<UrlHandlerPrefs::Match> matches;
  for (auto& handler : all_handlers.GetList()) {
    if (!handler.is_dict())
      continue;

    const std::string* const app_id = handler.FindStringKey(kAppId);
    if (!app_id || app_id->empty())
      continue;

    base::Optional<base::FilePath> profile_path =
        util::ValueToFilePath(handler.FindKey(kProfilePath));
    if (!profile_path)
      continue;

    if (origin_trimmed) {
      base::Optional<bool> has_wildcard =
          handler.FindBoolKey(kHasOriginWildcard);
      if (!has_wildcard || !*has_wildcard)
        continue;
    }

    // TODO(crbug/1072058): Filter results by matching against "paths" and
    // "exclude_paths" lists. This would give developers finer control of what
    // URLs trigger URL handling behavior.

    matches.emplace_back(*app_id, *profile_path);
  }
  return matches;
}

// Returns the URL handlers stored in |pref_value| that match |url|'s origin.
base::Optional<std::vector<UrlHandlerPrefs::Match>> FindMatches(
    const base::Value& pref_value,
    const GURL& url) {
  if (!pref_value.is_dict())
    return base::nullopt;

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque())
    return base::nullopt;

  if (origin.scheme() != "https")
    return base::nullopt;

  std::string origin_str = origin.Serialize();
  bool origin_trimmed(false);
  std::vector<UrlHandlerPrefs::Match> matches;
  for (;;) {
    const base::Value* const all_handlers = pref_value.FindListKey(origin_str);
    if (all_handlers) {
      DCHECK(UrlMatchesOrigin(url, origin_str, origin_trimmed));
      base::Optional<std::vector<UrlHandlerPrefs::Match>> matches_local =
          FilterMatches(*all_handlers, url, origin_trimmed);
      if (matches_local) {
        matches.insert(matches.end(),
                       std::make_move_iterator(matches_local->begin()),
                       std::make_move_iterator(matches_local->end()));
      }
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

base::Value NewHandler(const AppId& app_id,
                       const base::FilePath& profile_path,
                       const apps::UrlHandlerInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kAppId, app_id);
  value.SetKey(kProfilePath, util::FilePathToValue(profile_path));
  value.SetBoolKey(kHasOriginWildcard, info.has_origin_wildcard);
  // TODO(crbug/1072058): Set paths and exclude paths from origin association
  // data when it is available.
  base::Value paths(base::Value::Type::LIST);
  base::Value exclude_paths(base::Value::Type::LIST);
  value.SetKey(kPaths, std::move(paths));
  value.SetKey(kExcludePaths, std::move(exclude_paths));
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
      pref_value.RemoveKey(origin_value.first);
    }
  }
}
}  // namespace

UrlHandlerPrefs::Match::Match(const AppId& app_id,
                              const base::FilePath& profile_path)
    : app_id(app_id), profile_path(profile_path) {
  // Match should either be default constructed with both fields empty, or using
  // this constructor with both fields non-empty.
  DCHECK(!app_id.empty());
  DCHECK(!profile_path.empty());
}

UrlHandlerPrefs::UrlHandlerPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

void UrlHandlerPrefs::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterDictionaryPref(prefs::kWebAppsUrlHandlerInfo);
}

void UrlHandlerPrefs::AddWebApp(const AppId& app_id,
                                const base::FilePath& profile_path,
                                const apps::UrlHandlers& url_handlers) {
  if (profile_path.empty() || url_handlers.empty())
    return;

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsUrlHandlerInfo);
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

void UrlHandlerPrefs::RemoveWebApp(const AppId& app_id,
                                   const base::FilePath& profile_path) {
  if (app_id.empty() || profile_path.empty())
    return;

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  RemoveEntries(*pref_value, app_id, profile_path);
}

void UrlHandlerPrefs::RemoveProfile(const base::FilePath& profile_path) {
  if (profile_path.empty())
    return;

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  RemoveEntries(*pref_value, /*app_id*/ "", profile_path);
}

void UrlHandlerPrefs::Clear() {
  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  pref_value->DictClear();
}

base::Optional<std::vector<UrlHandlerPrefs::Match>>
UrlHandlerPrefs::FindMatchingUrlHandlers(const GURL& url) const {
  if (!url.is_valid())
    return base::nullopt;

  const base::Value* const pref_value =
      pref_service_->Get(prefs::kWebAppsUrlHandlerInfo);
  if (!pref_value || !pref_value->is_dict())
    return base::nullopt;

  return FindMatches(*pref_value, url);
}

}  // namespace web_app
