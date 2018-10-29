// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blacklist_manager.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"
#include "url/url_util.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherCondition;
using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherPortFilter;
using url_matcher::URLMatcherSchemeFilter;
using url_matcher::URLQueryElementMatcherCondition;

namespace policy {

namespace {

// List of schemes of URLs that should not be blocked by the "*" wildcard in
// the blacklist. Note that URLs with these schemes can still be blocked with
// a more specific filter e.g. "chrome-extension://*".
// The schemes are hardcoded here to avoid dependencies on //extensions and
// //chrome.
const char* kBypassBlacklistWildcardForSchemes[] = {
  // For internal extension URLs e.g. the Bookmark Manager and the File
  // Manager on Chrome OS.
  "chrome-extension",

  // NTP on Android.
  "chrome-native",

  // NTP on other platforms.
  "chrome-search",
};

// Maximum filters per policy. Filters over this index are ignored.
const size_t kMaxFiltersPerPolicy = 1000;

// Returns a blacklist based on the given |block| and |allow| pattern lists.
std::unique_ptr<URLBlacklist> BuildBlacklist(const base::ListValue* block,
                                             const base::ListValue* allow) {
  auto blacklist = std::make_unique<URLBlacklist>();
  blacklist->Block(block);
  blacklist->Allow(allow);
  return blacklist;
}

// Tokenise the parameter |query| and add appropriate query element matcher
// conditions to the |query_conditions|.
void ProcessQueryToConditions(
    url_matcher::URLMatcherConditionFactory* condition_factory,
    const std::string& query,
    bool allow,
    std::set<URLQueryElementMatcherCondition>* query_conditions) {
  url::Component query_left = url::MakeRange(0, query.length());
  url::Component key;
  url::Component value;
  // Depending on the filter type being black-list or white-list, the matcher
  // choose any or every match. The idea is a URL should be black-listed if
  // there is any occurrence of the key value pair. It should be white-listed
  // only if every occurrence of the key is followed by the value. This avoids
  // situations such as a user appending a white-listed video parameter in the
  // end of the query and watching a video of their choice (the last parameter
  // is ignored by some web servers like youtube's).
  URLQueryElementMatcherCondition::Type match_type =
      allow ? URLQueryElementMatcherCondition::MATCH_ALL
            : URLQueryElementMatcherCondition::MATCH_ANY;

  while (ExtractQueryKeyValue(query.data(), &query_left, &key, &value)) {
    URLQueryElementMatcherCondition::QueryElementType query_element_type =
        value.len ? URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE
                  : URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY;
    URLQueryElementMatcherCondition::QueryValueMatchType query_value_match_type;
    if (!value.len && key.len && query[key.end() - 1] == '*') {
      --key.len;
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX;
    } else if (value.len && query[value.end() - 1] == '*') {
      --value.len;
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX;
    } else {
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT;
    }
    query_conditions->insert(
        URLQueryElementMatcherCondition(query.substr(key.begin, key.len),
                                        query.substr(value.begin, value.len),
                                        query_value_match_type,
                                        query_element_type,
                                        match_type,
                                        condition_factory));
  }
}

bool BypassBlacklistWildcardForURL(const GURL& url) {
  const std::string& scheme = url.scheme();
  for (size_t i = 0; i < arraysize(kBypassBlacklistWildcardForSchemes); ++i) {
    if (scheme == kBypassBlacklistWildcardForSchemes[i])
      return true;
  }
  return false;
}

}  // namespace

struct URLBlacklist::FilterComponents {
  FilterComponents() : port(0), match_subdomains(true), allow(true) {}
  ~FilterComponents() = default;
  FilterComponents(const FilterComponents&) = delete;
  FilterComponents(FilterComponents&&) = default;
  FilterComponents& operator=(const FilterComponents&) = delete;
  FilterComponents& operator=(FilterComponents&&) = default;

  // Returns true if |this| represents the "*" filter in the blacklist.
  bool IsBlacklistWildcard() const {
    return !allow && host.empty() && scheme.empty() && path.empty() &&
           query.empty() && port == 0 && number_of_key_value_pairs == 0 &&
           match_subdomains;
  }

  std::string scheme;
  std::string host;
  uint16_t port;
  std::string path;
  std::string query;
  int number_of_key_value_pairs;
  bool match_subdomains;
  bool allow;
};

URLBlacklist::URLBlacklist() : id_(0), url_matcher_(new URLMatcher) {}

URLBlacklist::~URLBlacklist() {}

void URLBlacklist::AddFilters(bool allow, const base::ListValue* list) {
  URLMatcherConditionSet::Vector all_conditions;
  size_t size = std::min(kMaxFiltersPerPolicy, list->GetSize());
  std::string pattern;
  scoped_refptr<URLMatcherConditionSet> condition_set;
  for (size_t i = 0; i < size; ++i) {
    bool success = list->GetString(i, &pattern);
    DCHECK(success);
    FilterComponents components;
    components.allow = allow;
    if (!FilterToComponents(pattern,
                            &components.scheme,
                            &components.host,
                            &components.match_subdomains,
                            &components.port,
                            &components.path,
                            &components.query)) {
      LOG(ERROR) << "Invalid pattern " << pattern;
      continue;
    }

    condition_set = CreateConditionSet(
        url_matcher_.get(), ++id_, components.scheme, components.host,
        components.match_subdomains, components.port, components.path,
        components.query, allow);
    components.number_of_key_value_pairs =
        condition_set->query_conditions().size();
    all_conditions.push_back(std::move(condition_set));
    filters_[id_] = std::move(components);
  }
  url_matcher_->AddConditionSets(all_conditions);
}

void URLBlacklist::Block(const base::ListValue* filters) {
  AddFilters(false, filters);
}

void URLBlacklist::Allow(const base::ListValue* filters) {
  AddFilters(true, filters);
}

bool URLBlacklist::IsURLBlocked(const GURL& url) const {
  return URLBlacklist::GetURLBlacklistState(url) ==
         URLBlacklist::URLBlacklistState::URL_IN_BLACKLIST;
}

URLBlacklist::URLBlacklistState URLBlacklist::GetURLBlacklistState(
    const GURL& url) const {
  std::set<URLMatcherConditionSet::ID> matching_ids =
      url_matcher_->MatchURL(url);

  const FilterComponents* max = nullptr;
  for (auto id = matching_ids.begin(); id != matching_ids.end(); ++id) {
    auto it = filters_.find(*id);
    DCHECK(it != filters_.end());
    const FilterComponents& filter = it->second;
    if (!max || FilterTakesPrecedence(filter, *max))
      max = &filter;
  }

  // Default neutral.
  if (!max)
    return URLBlacklist::URLBlacklistState::URL_NEUTRAL_STATE;

  // Some of the internal Chrome URLs are not affected by the "*" in the
  // blacklist. Note that the "*" is the lowest priority filter possible, so
  // any higher priority filter will be applied first.
  if (max->IsBlacklistWildcard() && BypassBlacklistWildcardForURL(url))
    return URLBlacklist::URLBlacklistState::URL_IN_WHITELIST;

  return max->allow ?
      URLBlacklist::URLBlacklistState::URL_IN_WHITELIST :
      URLBlacklist::URLBlacklistState::URL_IN_BLACKLIST;
}

size_t URLBlacklist::Size() const {
  return filters_.size();
}

// static
bool URLBlacklist::FilterToComponents(const std::string& filter,
                                      std::string* scheme,
                                      std::string* host,
                                      bool* match_subdomains,
                                      uint16_t* port,
                                      std::string* path,
                                      std::string* query) {
  DCHECK(scheme);
  DCHECK(host);
  DCHECK(match_subdomains);
  DCHECK(port);
  DCHECK(path);
  DCHECK(query);
  url::Parsed parsed;
  const std::string lc_filter = base::ToLowerASCII(filter);
  const std::string url_scheme = url_formatter::SegmentURL(filter, &parsed);

  // Check if it's a scheme wildcard pattern. We support both versions
  // (scheme:* and scheme://*) the later being consistent with old filter
  // definitions.
  if (lc_filter == url_scheme + ":*" || lc_filter == url_scheme + "://*") {
    scheme->assign(url_scheme);
    host->clear();
    *match_subdomains = true;
    *port = 0;
    path->clear();
    query->clear();
    return true;
  }

  if (url_scheme == url::kFileScheme) {
    base::FilePath file_path;
    if (!net::FileURLToFilePath(GURL(filter), &file_path))
      return false;

    *scheme = url::kFileScheme;
    host->clear();
    *match_subdomains = true;
    *port = 0;
    *path = file_path.AsUTF8Unsafe();
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Separators have to be canonicalized on Windows.
    std::replace(path->begin(), path->end(), '\\', '/');
    *path = "/" + *path;
#endif
    return true;
  }

  // According to documentation host can't be empty.
  if (!parsed.host.is_nonempty())
    return false;

  if (parsed.scheme.is_nonempty())
    scheme->assign(url_scheme);
  else
    scheme->clear();

  host->assign(filter, parsed.host.begin, parsed.host.len);
  *host = base::ToLowerASCII(*host);
  // Special '*' host, matches all hosts.
  if (*host == "*") {
    host->clear();
    *match_subdomains = true;
  } else if (host->at(0) == '.') {
    // A leading dot in the pattern syntax means that we don't want to match
    // subdomains.
    host->erase(0, 1);
    *match_subdomains = false;
  } else {
    url::RawCanonOutputT<char> output;
    url::CanonHostInfo host_info;
    url::CanonicalizeHostVerbose(filter.c_str(), parsed.host, &output,
                                 &host_info);
    if (host_info.family == url::CanonHostInfo::NEUTRAL) {
      // We want to match subdomains. Add a dot in front to make sure we only
      // match at domain component boundaries.
      *host = "." + *host;
      *match_subdomains = true;
    } else {
      *match_subdomains = false;
    }
  }

  if (parsed.port.is_nonempty()) {
    int int_port;
    if (!base::StringToInt(filter.substr(parsed.port.begin, parsed.port.len),
                           &int_port)) {
      return false;
    }
    if (int_port <= 0 || int_port > std::numeric_limits<uint16_t>::max())
      return false;
    *port = int_port;
  } else {
    // Match any port.
    *port = 0;
  }

  if (parsed.path.is_nonempty())
    path->assign(filter, parsed.path.begin, parsed.path.len);
  else
    path->clear();

  if (parsed.query.is_nonempty())
    query->assign(filter, parsed.query.begin, parsed.query.len);
  else
    query->clear();

  return true;
}

// static
scoped_refptr<URLMatcherConditionSet> URLBlacklist::CreateConditionSet(
    URLMatcher* url_matcher,
    int id,
    const std::string& scheme,
    const std::string& host,
    bool match_subdomains,
    uint16_t port,
    const std::string& path,
    const std::string& query,
    bool allow) {
  URLMatcherConditionFactory* condition_factory =
      url_matcher->condition_factory();
  std::set<URLMatcherCondition> conditions;
  conditions.insert(match_subdomains ?
      condition_factory->CreateHostSuffixPathPrefixCondition(host, path) :
      condition_factory->CreateHostEqualsPathPrefixCondition(host, path));

  std::set<URLQueryElementMatcherCondition> query_conditions;
  if (!query.empty()) {
    ProcessQueryToConditions(
        condition_factory, query, allow, &query_conditions);
  }

  std::unique_ptr<URLMatcherSchemeFilter> scheme_filter;
  if (!scheme.empty())
    scheme_filter.reset(new URLMatcherSchemeFilter(scheme));

  std::unique_ptr<URLMatcherPortFilter> port_filter;
  if (port != 0) {
    std::vector<URLMatcherPortFilter::Range> ranges;
    ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    port_filter.reset(new URLMatcherPortFilter(ranges));
  }

  return base::MakeRefCounted<URLMatcherConditionSet>(
      id, conditions, query_conditions, std::move(scheme_filter),
      std::move(port_filter));
}

// static
bool URLBlacklist::FilterTakesPrecedence(const FilterComponents& lhs,
                                         const FilterComponents& rhs) {
  // The "*" wildcard is the lowest priority filter.
  if (rhs.IsBlacklistWildcard())
    return true;

  if (lhs.match_subdomains && !rhs.match_subdomains)
    return false;
  if (!lhs.match_subdomains && rhs.match_subdomains)
    return true;

  size_t host_length = lhs.host.length();
  size_t other_host_length = rhs.host.length();
  if (host_length != other_host_length)
    return host_length > other_host_length;

  size_t path_length = lhs.path.length();
  size_t other_path_length = rhs.path.length();
  if (path_length != other_path_length)
    return path_length > other_path_length;

  if (lhs.number_of_key_value_pairs != rhs.number_of_key_value_pairs)
    return lhs.number_of_key_value_pairs > rhs.number_of_key_value_pairs;

  if (lhs.allow && !rhs.allow)
    return true;

  return false;
}

URLBlacklistManager::URLBlacklistManager(PrefService* pref_service)
    : pref_service_(pref_service),
      blacklist_(new URLBlacklist),
      ui_weak_ptr_factory_(this) {
  // This class assumes that it is created on the same thread that
  // |pref_service_| lives on.
  ui_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  background_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT});

  pref_change_registrar_.Init(pref_service_);
  base::Closure callback = base::Bind(&URLBlacklistManager::ScheduleUpdate,
                                      base::Unretained(this));
  pref_change_registrar_.Add(policy_prefs::kUrlBlacklist, callback);
  pref_change_registrar_.Add(policy_prefs::kUrlWhitelist, callback);

  // Start enforcing the policies without a delay when they are present at
  // startup.
  if (pref_service_->HasPrefPath(policy_prefs::kUrlBlacklist) ||
      pref_service_->HasPrefPath(policy_prefs::kUrlWhitelist)) {
    SetBlacklist(
        BuildBlacklist(pref_service_->GetList(policy_prefs::kUrlBlacklist),
                       pref_service_->GetList(policy_prefs::kUrlWhitelist)));
  }
}

URLBlacklistManager::~URLBlacklistManager() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  pref_change_registrar_.RemoveAll();
}

void URLBlacklistManager::ScheduleUpdate() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Cancel pending updates, if any. This can happen if two preferences that
  // change the blacklist are updated in one message loop cycle. In those cases,
  // only rebuild the blacklist after all the preference updates are processed.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&URLBlacklistManager::Update,
                                           ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlacklistManager::Update() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  // The URLBlacklist is built in the background. Once it's ready, it is passed
  // to the URLBlacklistManager back on ui_task_runner_.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &BuildBlacklist,
          base::Owned(
              pref_service_->GetList(policy_prefs::kUrlBlacklist)->DeepCopy()),
          base::Owned(
              pref_service_->GetList(policy_prefs::kUrlWhitelist)->DeepCopy())),
      base::BindOnce(&URLBlacklistManager::SetBlacklist,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlacklistManager::SetBlacklist(
    std::unique_ptr<URLBlacklist> blacklist) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  blacklist_ = std::move(blacklist);
}

bool URLBlacklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Ignore blob scheme for two reasons:
  // 1) PlzNavigate uses it to deliver the response to the renderer.
  // 2) A whitelisted page can use blob URLs internally.
  return !url.SchemeIs(url::kBlobScheme) && blacklist_->IsURLBlocked(url);
}

URLBlacklist::URLBlacklistState URLBlacklistManager::GetURLBlacklistState(
    const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  return blacklist_->GetURLBlacklistState(url);
}

// static
void URLBlacklistManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(policy_prefs::kUrlBlacklist);
  registry->RegisterListPref(policy_prefs::kUrlWhitelist);
  registry->RegisterIntegerPref(
      policy_prefs::kSafeSitesFilterBehavior,
      static_cast<int>(SafeSitesFilterBehavior::kSafeSitesFilterDisabled));
}

}  // namespace policy
