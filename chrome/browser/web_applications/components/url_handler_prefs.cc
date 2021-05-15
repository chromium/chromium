// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_prefs.h"

#include <algorithm>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {
namespace url_handler_prefs {
namespace {

constexpr const char kAppId[] = "app_id";
constexpr const char kProfilePath[] = "profile_path";
constexpr const char kIncludePaths[] = "include_paths";
constexpr const char kExcludePaths[] = "exclude_paths";
constexpr const char kHasOriginWildcard[] = "has_origin_wildcard";
constexpr const char kDefaultPath[] = "/*";
constexpr const char kPath[] = "path";
constexpr const char kChoice[] = "choice";
constexpr const char kTimestamp[] = "timestamp";

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

// Returns true if |url_path| matches |path_pattern|. A prefix match is used if
// |path_pattern| ends with a '*' wildcard character. An exact match is used
// otherwise. |url_path| is a URL path from a fully specified URL.
// |path_pattern| is a URL path that can contain a wildcard postfix.
bool PathMatchesPathPattern(const std::string& url_path,
                            base::StringPiece path_pattern) {
  if (!path_pattern.empty() && path_pattern.back() == '*') {
    // Remove the wildcard and check if it's the same as the first several
    // characters of |url_path|.
    path_pattern = path_pattern.substr(0, path_pattern.length() - 1);
    if (base::StartsWith(url_path, path_pattern))
      return true;
  } else {
    // |path_pattern| doesn't contain a wildcard, check for an exact match.
    if (path_pattern == url_path)
      return true;
  }
  return false;
}

// Return true if |url_path| matches any path in |include_paths|. A path in
// |include_paths| can contain one wildcard '*' at the end.
// If any path matches, returns the best UrlHandlerSavedChoice found and
//  its associated timestamp through the |choice| and |time| output parameters.
// "Best" here is defined by this ordering: kInApp > kNone > kInBrowser.
// |url_path| always starts with a '/', as it's the result of GURL::path().
bool FindBestMatchingIncludePathChoice(const std::string& url_path,
                                       const base::Value& include_paths,
                                       UrlHandlerSavedChoice* choice,
                                       base::Time* time) {
  if (!include_paths.is_list())
    return false;

  UrlHandlerSavedChoice best_choice = UrlHandlerSavedChoice::kInBrowser;
  base::Time most_recent_timestamp;
  bool found_match = false;

  for (const auto& include_path_dict : include_paths.GetList()) {
    if (!include_path_dict.is_dict())
      continue;
    const std::string* include_path = include_path_dict.FindStringKey(kPath);
    if (!include_path)
      continue;
    const absl::optional<int> choice_opt =
        include_path_dict.FindIntKey(kChoice);
    if (!choice_opt)
      continue;
    // Check enum. bounds before casting.
    if (*choice_opt < 0 ||
        *choice_opt > static_cast<int>(UrlHandlerSavedChoice::kMax))
      continue;
    auto current_choice = static_cast<UrlHandlerSavedChoice>(*choice_opt);
    absl::optional<base::Time> current_timestamp =
        util::ValueToTime(include_path_dict.FindKey(kTimestamp));
    if (!current_timestamp)
      continue;

    if (PathMatchesPathPattern(url_path, *include_path)) {
      // If current_choice is better than best_choice, update best choice and
      // timestamp.
      bool update_best = current_choice > best_choice ||
                         // If current_choice and best_choice are equal, choose
                         // the one with the latest timestamp.
                         (best_choice == current_choice &&
                          current_timestamp > most_recent_timestamp);

      if (update_best) {
        best_choice = current_choice;
        most_recent_timestamp = *current_timestamp;
      }
      found_match = true;
    }
  }

  if (found_match) {
    *choice = best_choice;
    *time = most_recent_timestamp;
  }
  return found_match;
}

// Return true if |url_path| matches any path in |exclude_paths|. A path in
// |exclude_paths| can contain one wildcard '*' at the end.
bool ExcludePathMatches(const std::string& url_path,
                        const base::Value& exclude_paths) {
  if (!exclude_paths.is_list())
    return false;

  for (const auto& exclude_path : exclude_paths.GetList()) {
    if (!exclude_path.is_string())
      continue;
    if (PathMatchesPathPattern(url_path, exclude_path.GetString()))
      return true;
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

    absl::optional<base::FilePath> profile_path =
        util::ValueToFilePath(handler.FindKey(kProfilePath));
    if (!profile_path || profile_path->empty())
      continue;

    if (origin_trimmed) {
      absl::optional<bool> has_wildcard =
          handler.FindBoolKey(kHasOriginWildcard);
      if (!has_wildcard || !*has_wildcard)
        continue;
    }

    const std::string& url_path = url.path();
    const base::Value* const include_paths = handler.FindListKey(kIncludePaths);

    bool include_paths_exist = include_paths && include_paths->is_list() &&
                               !include_paths->GetList().empty();

    UrlHandlerSavedChoice best_choice = UrlHandlerSavedChoice::kNone;
    base::Time latest_timestamp = base::Time::Min();
    if (include_paths_exist &&
        !FindBestMatchingIncludePathChoice(url_path, *include_paths,
                                           &best_choice, &latest_timestamp)) {
      continue;
    }

    const base::Value* const exclude_paths = handler.FindListKey(kExcludePaths);
    bool exclude_paths_exist = exclude_paths && exclude_paths->is_list() &&
                               !exclude_paths->GetList().empty();
    if (exclude_paths_exist && ExcludePathMatches(url_path, *exclude_paths))
      continue;

    // Do not include a match if it should open in a normal browser tab.
    if (best_choice == UrlHandlerSavedChoice::kInBrowser)
      continue;

    matches.emplace_back(*profile_path, *app_id, url, best_choice,
                         latest_timestamp);
  }
}

// If one or more match should open in app, keep the most recent one and remove
// the others. Also remove matches with no saved choice.
// Otherwise, any remaining matches should all have no saved choice.
// Any match that should open in browser have already been removed and should
// not be found in |matches|.
void FilterBySavedChoice(std::vector<UrlHandlerLaunchParams>& matches) {
  for (const UrlHandlerLaunchParams& params : matches)
    DCHECK(params.saved_choice != UrlHandlerSavedChoice::kInBrowser);

  // Are there matches that open in app? Which is the most recently saved?
  bool has_in_app = false;
  base::Time most_recent_time = base::Time::Min();
  size_t most_recent_pos = 0;
  for (size_t i = 0; i < matches.size(); i++) {
    const UrlHandlerLaunchParams& params = matches[i];
    if (params.saved_choice == UrlHandlerSavedChoice::kInApp) {
      has_in_app = true;
      if (params.saved_choice_timestamp > most_recent_time) {
        most_recent_time = params.saved_choice_timestamp;
        most_recent_pos = i;
      }
    }
  }

  // Only keep the most recently saved match that opens in app.
  if (has_in_app)
    matches = {std::move(matches[most_recent_pos])};

  // Since no matches opened in browser to begin with and now no matches open in
  // app, any remaining matches must have no saved choice.
}

void FindMatchesImpl(const base::Value& pref_value,
                     const GURL& url,
                     std::vector<UrlHandlerLaunchParams>& matches,
                     const std::string& origin_str,
                     const bool origin_trimmed) {
  const base::Value* const all_handlers = pref_value.FindListKey(origin_str);
  if (all_handlers) {
    DCHECK(UrlMatchesOrigin(url, origin_str, origin_trimmed));
    FilterAndAddMatches(*all_handlers, url, origin_trimmed, matches);
    FilterBySavedChoice(matches);
  }
}

// Helper function that runs |op| repeatedly with shorter versions of
// |origin_str|. This helps match URLs to entries keyed by broader origins.
template <typename Operation>
void TryDifferentOriginSubstrings(std::string origin_str, Operation op) {
  bool origin_trimmed = false;
  while (true) {
    op(origin_str, origin_trimmed);

    // Try to shorten origin_str to the next origin suffix by removing 1
    // sub-domain. This enables matching against origins that contain wildcard
    // prefixes. As these origins with wildcard prefixes could be of different
    // lengths and yet match the initial origin_str, every suffix is processed.
    auto found = origin_str.find('.');
    if (found != std::string::npos) {
      // Trim origin to after next '.' character if there is one.
      origin_str = base::StrCat({"https://", origin_str.substr(found + 1)});
      origin_trimmed = true;
      // Do not early return here. There could be other apps that match using
      // origin wildcard.
    } else {
      // There is no more '.'. Stop looking.
      break;
    }
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

  // FindMatchesImpl accumulates results to |matches|.
  std::string origin_str = origin.Serialize();
  TryDifferentOriginSubstrings(
      origin_str, [&pref_value, &url, &matches](const std::string& origin_str,
                                                bool origin_trimmed) {
        FindMatchesImpl(pref_value, url, matches, origin_str, origin_trimmed);
      });
  return matches;
}

base::Value GetIncludePathsValue(
    const std::vector<std::string>& include_paths) {
  base::Value value(base::Value::Type::LIST);
  // When no "paths" are specified in web-app-origin-association, all include
  // paths are allowed.
  for (const auto& include_path : include_paths.empty()
                                      ? std::vector<std::string>({kDefaultPath})
                                      : include_paths) {
    base::Value path_dict(base::Value::Type::DICTIONARY);
    path_dict.SetStringKey(kPath, include_path);
    path_dict.SetIntKey(kChoice,
                        static_cast<int>(UrlHandlerSavedChoice::kNone));
    path_dict.SetKey(kTimestamp, util::TimeToValue(base::Time::Min()));
    value.Append(std::move(path_dict));
  }
  return value;
}

base::Value GetExcludePathsValue(
    const std::vector<std::string>& exclude_paths) {
  base::Value value(base::Value::Type::LIST);
  for (const auto& exclude_path : exclude_paths) {
    value.Append(exclude_path);
  }
  return value;
}

base::Value NewHandler(const AppId& app_id,
                       const base::FilePath& profile_path,
                       const apps::UrlHandlerInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kAppId, app_id);
  value.SetKey(kProfilePath, util::FilePathToValue(profile_path));
  value.SetBoolKey(kHasOriginWildcard, info.has_origin_wildcard);
  // Set include_paths and exclude paths from associated app.
  value.SetKey(kIncludePaths, GetIncludePathsValue(info.paths));
  value.SetKey(kExcludePaths, GetExcludePathsValue(info.exclude_paths));
  return value;
}

// If |match_app_id| is true, returns true if |handler| has dict. values equal
// to |app_id| and |profile_path|. If |match_app_id| is false, only compare
// |profile_path|.
bool IsHandlerForApp(const AppId& app_id,
                     const base::FilePath& profile_path,
                     bool match_app_id,
                     const base::Value& handler) {
  const std::string* const handler_app_id = handler.FindStringKey(kAppId);
  absl::optional<base::FilePath> handler_profile_path =
      util::ValueToFilePath(handler.FindKey(kProfilePath));

  if (!handler_app_id || !handler_profile_path)
    return false;

  if (*handler_profile_path != profile_path)
    return false;

  return !match_app_id || *handler_app_id == app_id;
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

// Sets |choice| on every path in |include_paths| that matches |url|.
void UpdateSavedChoice(base::Value& include_paths,
                       const GURL& url,
                       UrlHandlerSavedChoice choice,
                       const base::Time& time) {
  // |include_paths| is a list of include path dicts. Eg:
  // [ {
  //    "choice": 0,
  //    "path": "/abc",
  //    "timestamp": "-9223372036854775808"
  // } ]
  auto include_paths_list = include_paths.TakeList();
  for (base::Value& include_path_dict : include_paths_list) {
    if (!include_path_dict.is_dict())
      continue;
    const std::string* path = include_path_dict.FindStringKey(kPath);
    if (!path)
      continue;

    // Any matching path dict. will be updated with the input choice and
    // timestamp.
    if (PathMatchesPathPattern(url.path(), *path)) {
      include_path_dict.SetIntKey(kChoice, static_cast<int>(choice));
      include_path_dict.SetKey(kTimestamp, util::TimeToValue(time));
    }
  }
  include_paths = base::Value(std::move(include_paths_list));
}

void SaveChoiceImpl(const AppId* app_id,
                    const base::FilePath* profile_path,
                    const GURL& url,
                    const UrlHandlerSavedChoice choice,
                    const base::Time& time,
                    base::Value& pref_value,
                    const std::string& origin_str,
                    const bool origin_trimmed) {
  base::Value* const handlers_mutable = pref_value.FindListKey(origin_str);
  if (handlers_mutable) {
    DCHECK(UrlMatchesOrigin(url, origin_str, origin_trimmed));
    base::Value::ListStorage handlers = handlers_mutable->TakeList();
    for (auto& handler : handlers) {
      if (!handler.is_dict())
        continue;
      const std::string* const handler_app_id = handler.FindStringKey(kAppId);
      absl::optional<base::FilePath> handler_profile_path =
          util::ValueToFilePath(handler.FindKey(kProfilePath));
      if (!handler_app_id || !handler_profile_path)
        continue;

      if (choice == UrlHandlerSavedChoice::kInApp) {
        if (*handler_app_id != *app_id ||
            *handler_profile_path != *profile_path) {
          continue;
        }
      }

      base::Value* const include_paths = handler.FindListKey(kIncludePaths);
      if (include_paths)
        UpdateSavedChoice(*include_paths, url, choice, time);
    }
    *handlers_mutable = base::Value(std::move(handlers));
  }
}

// Saves |choice| and |time| to all handler include_paths that match |app_id|,
// |profile_path|, and |url|. |url| provides both origin and path for matching.
void SaveChoice(PrefService* local_state,
                const AppId* app_id,
                const base::FilePath* profile_path,
                const GURL& url,
                const UrlHandlerSavedChoice choice,
                const base::Time& time) {
  DCHECK(url.is_valid());
  DCHECK(local_state);
  DCHECK(choice != UrlHandlerSavedChoice::kNone);
  // |app_id| and |profile_path| are not needed when choice == kInBrowser.
  DCHECK(choice != UrlHandlerSavedChoice::kInBrowser ||
         (app_id == nullptr && profile_path == nullptr));

  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque())
    return;

  if (origin.scheme() != url::kHttpsScheme)
    return;

  std::string origin_str = origin.Serialize();

  // SaveChoiceImpl modifies prefs but produces no output.
  TryDifferentOriginSubstrings(
      origin_str, [app_id, profile_path, &url, choice, &time, pref_value](
                      const std::string& origin_str, bool origin_trimmed) {
        SaveChoiceImpl(app_id, profile_path, url, choice, time, *pref_value,
                       origin_str, origin_trimmed);
      });
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
  // TODO(crbug/1072058): Retain saved choices where possible if there are
  // updates to 'url_handlers'.
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

void SaveOpenInApp(PrefService* local_state,
                   const AppId& app_id,
                   const base::FilePath& profile_path,
                   const GURL& url,
                   const base::Time& time) {
  DCHECK(!profile_path.empty());
  DCHECK(!app_id.empty());
  SaveChoice(local_state, &app_id, &profile_path, url,
             UrlHandlerSavedChoice::kInApp, time);
}

void SaveOpenInBrowser(PrefService* local_state,
                       const GURL& url,
                       const base::Time& time) {
  SaveChoice(local_state, /*app_id=*/nullptr, /*profile_path=*/nullptr, url,
             UrlHandlerSavedChoice::kInBrowser, time);
}

}  // namespace url_handler_prefs
}  // namespace web_app
