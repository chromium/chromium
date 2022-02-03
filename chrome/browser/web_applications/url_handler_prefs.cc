// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/url_handler_prefs.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
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

  for (const auto& include_path_dict : include_paths.GetListDeprecated()) {
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
        base::ValueToTime(include_path_dict.FindKey(kTimestamp));
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

  for (const auto& exclude_path : exclude_paths.GetListDeprecated()) {
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
void FilterAndAddMatches(const base::Value& all_handlers,
                         const GURL& url,
                         bool origin_trimmed,
                         std::vector<UrlHandlerLaunchParams>& matches) {
  if (!all_handlers.is_list())
    return;

  for (const base::Value& handler : all_handlers.GetListDeprecated()) {
    absl::optional<const HandlerView> handler_view =
        GetConstHandlerView(handler);
    if (!handler_view)
      continue;

    // |origin_trimmed| indicates if the input URL's origin had to be shortened
    // to find a matching key. If true, filter out any matches that did not
    // allow an origin prefix wildcard in their manifest.
    if (origin_trimmed && !handler_view->has_origin_wildcard)
      continue;

    const std::string& url_path = url.path();
    bool include_paths_exist =
        !handler_view->include_paths.GetListDeprecated().empty();
    UrlHandlerSavedChoice best_choice = UrlHandlerSavedChoice::kNone;
    base::Time latest_timestamp = base::Time::Min();
    if (include_paths_exist && !FindBestMatchingIncludePathChoice(
                                   url_path, handler_view->include_paths,
                                   &best_choice, &latest_timestamp)) {
      continue;
    }

    bool exclude_paths_exist =
        !handler_view->exclude_paths.GetListDeprecated().empty();
    if (exclude_paths_exist &&
        ExcludePathMatches(url_path, handler_view->exclude_paths)) {
      continue;
    }

    matches.emplace_back(handler_view->profile_path, handler_view->app_id, url,
                         best_choice, latest_timestamp);
  }
}

// Find the most recent match. If it is saved as kInBrowser, preferred choice
// is the browser so no matches should be returned; If saved as kNone, all the
// matches should be returned so the user can make a new saved choice; If
// kInApp, only returned the app match as it is the saved choice.
void FilterBySavedChoice(std::vector<UrlHandlerLaunchParams>& matches) {
  if (matches.empty())
    return;

  // Record the most recent match. If two matches have the same timestamp,
  // prefer the one with a higher saved_choice value.
  auto most_recent_match_iterator = base::ranges::max_element(
      matches, [](const UrlHandlerLaunchParams& match1,
                  const UrlHandlerLaunchParams& match2) {
        if (match1.saved_choice_timestamp > match2.saved_choice_timestamp)
          return false;
        if (match1.saved_choice_timestamp < match2.saved_choice_timestamp)
          return true;

        return match1.saved_choice < match2.saved_choice;
      });

  switch (most_recent_match_iterator->saved_choice) {
    case UrlHandlerSavedChoice::kInApp:
      matches = {std::move(*most_recent_match_iterator)};
      break;
    case UrlHandlerSavedChoice::kInBrowser:
      matches = {};
      break;
    case UrlHandlerSavedChoice::kNone:
      // `matches` already contain all matches. Do not modify.
      break;
  }
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

base::Value GetIncludePathsValue(const std::vector<std::string>& include_paths,
                                 const base::Time& time) {
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
    path_dict.SetKey(kTimestamp, base::TimeToValue(time));
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
                       const apps::UrlHandlerInfo& info,
                       const base::Time& time) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kAppId, app_id);
  value.SetKey(kProfilePath, base::FilePathToValue(profile_path));
  value.SetBoolKey(kHasOriginWildcard, info.has_origin_wildcard);
  // Set include_paths and exclude paths from associated app.
  value.SetKey(kIncludePaths, GetIncludePathsValue(info.paths, time));
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
  auto handler_view = GetConstHandlerView(handler);
  if (!handler_view)
    return false;

  if (handler_view->profile_path != profile_path)
    return false;

  return !match_app_id || handler_view->app_id == app_id;
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
    base::Value::ListStorage handlers =
        std::move(origin_value.second).TakeListDeprecated();
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

using PathSet = base::flat_set<std::string>;

// Sets |choice| on every include path in |all_include_paths| where the path
// exists in |updated_include_paths|.
void UpdateSavedChoiceInIncludePaths(const PathSet& updated_include_paths,
                                     UrlHandlerSavedChoice choice,
                                     const base::Time& time,
                                     base::Value& all_include_paths) {
  // |all_include_paths| is a list of include path dicts. Eg:
  // [ {
  //    "choice": 0,
  //    "path": "/abc",
  //    "timestamp": "-9223372036854775808"
  // } ]
  auto include_paths_list = std::move(all_include_paths).TakeListDeprecated();
  for (base::Value& include_path_dict : include_paths_list) {
    if (!include_path_dict.is_dict())
      continue;
    const std::string* path = include_path_dict.FindStringKey(kPath);
    if (!path)
      continue;

    if (updated_include_paths.contains(*path)) {
      include_path_dict.SetIntKey(kChoice, static_cast<int>(choice));
      include_path_dict.SetKey(kTimestamp, base::TimeToValue(time));
    }
  }
  all_include_paths = base::Value(std::move(include_paths_list));
}

// Sets |choice| on every path in |include_paths| that matches |url|. Returns
// a set of paths that are updated.
PathSet UpdateSavedChoice(const GURL& url,
                          UrlHandlerSavedChoice choice,
                          const base::Time& time,
                          base::Value& include_paths) {
  // |include_paths| is a list of include path dicts. Eg:
  // [ {
  //    "choice": 0,
  //    "path": "/abc",
  //    "timestamp": "-9223372036854775808"
  // } ]
  auto include_paths_list = std::move(include_paths).TakeListDeprecated();
  std::vector<std::string> updated_include_paths;
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
      include_path_dict.SetKey(kTimestamp, base::TimeToValue(time));
      updated_include_paths.push_back(*path);
    }
  }
  include_paths = base::Value(std::move(include_paths_list));
  return std::move(updated_include_paths);
}

// Update the save choice on every include path that matches the |url|.
void SaveChoiceToAllMatchingIncludePaths(const GURL& url,
                                         const UrlHandlerSavedChoice choice,
                                         const base::Time& time,
                                         base::Value::ListStorage& handlers) {
  for (auto& handler : handlers) {
    auto handler_view = GetHandlerView(handler);
    if (!handler_view)
      continue;

    UpdateSavedChoice(url, choice, time, handler_view->include_paths);
  }
}

bool AppIdAndProfileMatch(const AppId* app_id,
                          const base::FilePath* profile_path,
                          const std::string& handler_app_id,
                          const base::FilePath& handler_profile_path) {
  return (*app_id == handler_app_id) && (*profile_path == handler_profile_path);
}

// Update the matching include paths' saved choice where app id and profile
// path match |app_id| and |profile_path|. Return which include paths are
// updated.
PathSet SaveInAppChoiceToSelectedApp(const AppId* app_id,
                                     const base::FilePath* profile_path,
                                     const GURL& url,
                                     const base::Time& time,
                                     base::Value::ListStorage& handlers) {
  PathSet updated_include_paths;
  for (auto& handler : handlers) {
    auto handler_view = GetHandlerView(handler);
    if (!handler_view ||
        !AppIdAndProfileMatch(app_id, profile_path, handler_view->app_id,
                              handler_view->profile_path)) {
      continue;
    }

    PathSet updated_paths = UpdateSavedChoice(
        url, UrlHandlerSavedChoice::kInApp, time, handler_view->include_paths);
    updated_include_paths.insert(updated_paths.begin(), updated_paths.end());
  }
  return updated_include_paths;
}

// Find include paths in |updated_include_paths| from apps that don't match
// |app_id| and |profile_path|. Reset the saved choice of these to kNone so
// they don't conflict with the app choice that was just saved.
void ResetSavedChoiceInOtherApps(const AppId* app_id,
                                 const base::FilePath* profile_path,
                                 const base::Time& time,
                                 PathSet updated_include_paths,
                                 base::Value::ListStorage& handlers) {
  for (auto& handler : handlers) {
    auto handler_view = GetHandlerView(handler);
    if (!handler_view ||
        AppIdAndProfileMatch(app_id, profile_path, handler_view->app_id,
                             handler_view->profile_path)) {
      continue;
    }

    UpdateSavedChoiceInIncludePaths(updated_include_paths,
                                    UrlHandlerSavedChoice::kNone, time,
                                    handler_view->include_paths);
  }
}

void SaveAppChoice(const AppId* app_id,
                   const base::FilePath* profile_path,
                   const GURL& url,
                   const base::Time& time,
                   base::Value::ListStorage& handlers) {
  PathSet updated_include_paths =
      SaveInAppChoiceToSelectedApp(app_id, profile_path, url, time, handlers);

  if (updated_include_paths.empty())
    return;

  ResetSavedChoiceInOtherApps(app_id, profile_path, time,
                              std::move(updated_include_paths), handlers);
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
  if (!handlers_mutable)
    return;

  DCHECK(UrlMatchesOrigin(url, origin_str, origin_trimmed));
  base::Value::ListStorage handlers =
      std::move(*handlers_mutable).TakeListDeprecated();

  if (choice == UrlHandlerSavedChoice::kInApp) {
    SaveAppChoice(app_id, profile_path, url, time, handlers);
  } else {
    SaveChoiceToAllMatchingIncludePaths(url, choice, time, handlers);
  }

  *handlers_mutable = base::Value(std::move(handlers));
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

bool ShouldUpdateIncludePaths(const base::Value& current_handler,
                              const base::Value& new_handler) {
  const base::Value* include_paths_lh =
      current_handler.FindListKey(kIncludePaths);
  const base::Value* include_paths_rh = new_handler.FindListKey(kIncludePaths);
  if (!include_paths_lh || !include_paths_rh)
    return true;

  base::Value::ConstListView include_paths_list_lh =
      include_paths_lh->GetListDeprecated();
  base::Value::ConstListView include_paths_list_rh =
      include_paths_rh->GetListDeprecated();
  if (include_paths_list_lh.size() != include_paths_list_rh.size())
    return true;

  for (size_t i = 0; i < include_paths_list_lh.size(); i++) {
    DCHECK(include_paths_list_lh[i].is_dict());
    DCHECK(include_paths_list_rh[i].is_dict());
    const std::string* path_lh = include_paths_list_lh[i].FindStringKey(kPath);
    const std::string* path_rh = include_paths_list_rh[i].FindStringKey(kPath);
    if (!path_lh || !path_rh)
      return true;
    if (*path_lh != *path_rh)
      return true;
  }
  return false;
}

// Update 'include_paths' in 'current_handler' from 'include_paths' in
// 'new_handler'. Update does not happen if 'include_paths' in both are
// identical. 'choice' and 'timestamp' are not compared to determine
// equivalence. Both handler values follow the format:
// {
//     "app_id": "qruhrugqrgjdsdfhjghjrghjhdfgaaamenww",
//     "profile_path": "C:\\Users\\alias\\Profile\\Default",
//     "has_origin_wildcard": true,
//     "include_paths": [
//         {
//           "path": "/*",
//           "choice": 2,  // kInApp
//           // "2000-01-01 00:00:00.000 UTC"
//           "timestamp": "12591158400000000"
//         }
//     ],
//     "exclude_paths": ["/abc"],
// }
void MaybeUpdateIncludePaths(base::Value& current_handler,
                             base::Value& new_handler) {
  if (ShouldUpdateIncludePaths(current_handler, new_handler)) {
    base::Value* new_include_paths = new_handler.FindListKey(kIncludePaths);
    if (new_include_paths) {
      current_handler.SetKey(kIncludePaths, std::move(*new_include_paths));
    } else {
      current_handler.SetKey(kIncludePaths,
                             base::Value(base::Value::Type::LIST));
    }
  }
}

// Updates 'exclude_paths' in 'current_handler' from 'exclude_paths' in
// 'new_handler'. 'exclude_paths' can be replaced directly because it stores no
// user preferences.
void UpdateExcludePaths(base::Value& current_handler,
                        base::Value& new_handler) {
  base::Value* new_exclude_paths = new_handler.FindListKey(kExcludePaths);
  if (new_exclude_paths) {
    current_handler.SetKey(kExcludePaths, std::move(*new_exclude_paths));
  } else {
    current_handler.SetKey(kExcludePaths, base::Value(base::Value::Type::LIST));
  }
}

// Returns true if 'handler_lh' and 'handler_rh' have identical app_id,
// profile_path, and has_origin_wildcard values.
bool HasExpectedIdenticalFields(const base::Value& handler_lh,
                                const base::Value& handler_rh) {
  auto handler_view_lh = GetConstHandlerView(handler_lh);
  auto handler_view_rh = GetConstHandlerView(handler_rh);
  if (!handler_view_lh || !handler_view_rh)
    return false;

  if (handler_view_lh->app_id != handler_view_rh->app_id)
    return false;
  if (handler_view_lh->profile_path != handler_view_rh->profile_path)
    return false;

  if (handler_view_lh->has_origin_wildcard !=
      handler_view_rh->has_origin_wildcard) {
    return false;
  }

  return true;
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterDictionaryPref(prefs::kWebAppsUrlHandlerInfo);
}

void AddWebApp(PrefService* local_state,
               const AppId& app_id,
               const base::FilePath& profile_path,
               const apps::UrlHandlers& url_handlers,
               const base::Time& time) {
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

    base::Value new_handler(
        NewHandler(app_id, profile_path, handler_info, time));
    base::Value* const handlers_mutable =
        pref_value->FindListKey(origin.Serialize());
    // One or more apps are already associated with this origin.
    if (handlers_mutable) {
      base::Value::ListStorage handlers =
          std::move(*handlers_mutable).TakeListDeprecated();
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
                  apps::UrlHandlers new_url_handlers,
                  const base::Time& time) {
  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;

  // In order to update data in URL handler prefs relevant to 'app_id' and
  // 'profile_path', perform an exhaustive search of all handler entries under
  // all keys. The previous url_handlers data could have had entries under any
  // origin key.
  std::vector<std::string> origins_to_remove;
  for (auto origin_value : pref_value->DictItems()) {
    const std::string& origin_str = origin_value.first;
    base::Value::ListStorage curent_handlers =
        std::move(origin_value.second).TakeListDeprecated();

    // Remove any existing handler values that were written previously for the
    // same app_id and profile but are no longer found in 'new_url_handlers'.
    curent_handlers.erase(
        base::ranges::remove_if(
            curent_handlers,
            // Returns true if 'current_handler' should be removed because it
            // was previously added for 'app_id' and 'profile_path' but is no
            // longer found in 'new_url_handlers' from the update.
            [&app_id, &profile_path, &new_url_handlers, &time,
             &origin_str](base::Value& current_handler) {
              if (!IsHandlerForApp(app_id, profile_path,
                                   /*match_app_id=*/true, current_handler)) {
                return false;
              }

              // Determine if 'current_handler' value has a corresponding
              // UrlHandlerInfo in 'new_url_handlers'. If not, it is no longer
              // relevant to the updated app and can be removed.
              const auto same_origin_it = base::ranges::find_if(
                  new_url_handlers,
                  [&current_handler,
                   &origin_str](const apps::UrlHandlerInfo& new_handler) {
                    if (origin_str != new_handler.origin.Serialize())
                      return false;
                    absl::optional<bool> current_has_origin_wildcard =
                        current_handler.FindBoolKey(kHasOriginWildcard);
                    if (!current_has_origin_wildcard)
                      return false;
                    if (*current_has_origin_wildcard !=
                        new_handler.has_origin_wildcard) {
                      return false;
                    }
                    return true;
                  });
              if (same_origin_it == new_url_handlers.end())
                return true;

              // If include_paths or exclude_paths have changed, replace the
              // current handler value with the new handler value.
              base::Value new_handler =
                  NewHandler(app_id, profile_path, *same_origin_it, time);

              // 'exclude_paths' can be updated without invalidating the user
              // preferences that are stored within include_paths.
              DCHECK(HasExpectedIdenticalFields(current_handler, new_handler));
              MaybeUpdateIncludePaths(current_handler, new_handler);
              UpdateExcludePaths(current_handler, new_handler);

              // Remove new handler from container now that it has been updated
              // in prefs.
              new_url_handlers.erase(same_origin_it);

              return false;
            }),
        curent_handlers.end());

    // Replace list if it contains entries or remove its origin key from prefs.
    if (!curent_handlers.empty()) {
      origin_value.second = base::Value(std::move(curent_handlers));
    } else {
      origins_to_remove.push_back(origin_value.first);
    }
  }

  // Remove any origin keys that have no more entries.
  for (const auto& origin_to_remove : origins_to_remove)
    pref_value->RemoveKey(origin_to_remove);

  // Add the remaining items in 'new_url_handlers'.
  AddWebApp(local_state, app_id, profile_path, new_url_handlers, time);
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

bool IsHandlerForProfile(const base::Value& handler,
                         const base::FilePath& profile_path) {
  auto handler_view = GetConstHandlerView(handler);
  if (!handler_view)
    return false;

  return handler_view->profile_path == profile_path;
}

bool ProfileHasUrlHandlers(PrefService* local_state,
                           const base::FilePath& profile_path) {
  const base::Value* const pref_value =
      local_state->Get(prefs::kWebAppsUrlHandlerInfo);
  if (!pref_value || !pref_value->is_dict())
    return false;

  for (const auto origin_value : pref_value->DictItems()) {
    for (const auto& handler : origin_value.second.GetListDeprecated()) {
      if (IsHandlerForProfile(handler, profile_path))
        return true;
    }
  }
  return false;
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

void ResetSavedChoice(PrefService* local_state,
                      const absl::optional<std::string>& app_id,
                      const base::FilePath& profile_path,
                      const std::string& origin,
                      bool has_origin_wildcard,
                      const std::string& url_path,
                      const base::Time& time) {
  DictionaryPrefUpdate update(local_state, prefs::kWebAppsUrlHandlerInfo);
  base::Value* const pref_value = update.Get();
  if (!pref_value || !pref_value->is_dict())
    return;
  base::Value* const handlers_mutable = pref_value->FindListKey(origin);
  if (!handlers_mutable)
    return;

  for (auto& handler : handlers_mutable->GetListDeprecated()) {
    auto handler_view = GetHandlerView(handler);
    if (!handler_view)
      continue;
    if (handler_view->profile_path != profile_path)
      continue;
    // Do not filter by app_id if no value is provided.
    if (app_id && handler_view->app_id != *app_id)
      continue;
    if (handler_view->has_origin_wildcard != has_origin_wildcard)
      continue;

    // Reset the choice and timestamp in every include_paths dict. where the
    // path member matches |url_path|.
    UpdateSavedChoiceInIncludePaths(PathSet({url_path}),
                                    UrlHandlerSavedChoice::kNone, time,
                                    handler_view->include_paths);
  }
}

absl::optional<const HandlerView> GetConstHandlerView(
    const base::Value& handler) {
  if (!handler.is_dict())
    return absl::nullopt;

  const std::string* const handler_app_id = handler.FindStringKey(kAppId);
  if (!handler_app_id)
    return absl::nullopt;

  absl::optional<base::FilePath> handler_profile_path =
      base::ValueToFilePath(handler.FindKey(kProfilePath));
  if (!handler_profile_path)
    return absl::nullopt;

  absl::optional<bool> has_origin_wildcard =
      handler.FindBoolKey(kHasOriginWildcard);
  if (!has_origin_wildcard)
    return absl::nullopt;

  base::Value* include_paths =
      const_cast<base::Value&>(handler).FindListKey(kIncludePaths);
  if (!include_paths || !include_paths->is_list())
    return absl::nullopt;

  base::Value* exclude_paths =
      const_cast<base::Value&>(handler).FindListKey(kExcludePaths);
  if (!exclude_paths || !exclude_paths->is_list())
    return absl::nullopt;

  HandlerView handler_view = {
      *handler_app_id,      handler_profile_path.value(),
      *has_origin_wildcard, *include_paths,
      *exclude_paths,
  };

  return handler_view;
}

absl::optional<HandlerView> GetHandlerView(base::Value& handler) {
  absl::optional<const HandlerView> handler_view = GetConstHandlerView(handler);
  if (!handler_view)
    return absl::nullopt;

  return const_cast<HandlerView&>(*handler_view);
}

}  // namespace url_handler_prefs
}  // namespace web_app
