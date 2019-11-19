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

using url_util::CreateConditionSet;
using url_util::FilterComponents;
using url_util::FilterToComponents;

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

// Returns a blacklist based on the given |block| and |allow| pattern lists.
std::unique_ptr<URLBlacklist> BuildBlacklist(const base::ListValue* block,
                                             const base::ListValue* allow) {
  auto blacklist = std::make_unique<URLBlacklist>();
  blacklist->Block(block);
  blacklist->Allow(allow);
  return blacklist;
}

bool BypassBlacklistWildcardForURL(const GURL& url) {
  const std::string& scheme = url.scheme();
  for (size_t i = 0; i < base::size(kBypassBlacklistWildcardForSchemes); ++i) {
    if (scheme == kBypassBlacklistWildcardForSchemes[i])
      return true;
  }
  return false;
}

}  // namespace

URLBlacklist::URLBlacklist() : id_(0), url_matcher_(new URLMatcher) {}

URLBlacklist::~URLBlacklist() {}

void URLBlacklist::Block(const base::ListValue* filters) {
  url_util::AddFilters(url_matcher_.get(), false, &id_, filters, &filters_);
}

void URLBlacklist::Allow(const base::ListValue* filters) {
  url_util::AddFilters(url_matcher_.get(), true, &id_, filters, &filters_);
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
    : pref_service_(pref_service), blacklist_(new URLBlacklist) {
  // This class assumes that it is created on the same thread that
  // |pref_service_| lives on.
  ui_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  background_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT});

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
  // 1) Its used to deliver the response to the renderer.
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
