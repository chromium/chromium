// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_manager.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_IOS)
#include <string_view>

#include "base/strings/string_util.h"
#endif

namespace policy {

using url_matcher::URLMatcher;
using url_matcher::util::FilterComponents;

namespace {

// List of schemes of URLs that should not be blocked by the "*" wildcard in
// the blocklist. Note that URLs with these schemes can still be blocked with
// a more specific filter e.g. "chrome-extension://*".
// The schemes are hardcoded here to avoid dependencies on //extensions and
// //chrome.
const char* kBypassBlocklistWildcardForSchemes[] = {
    // For internal extension URLs e.g. the Bookmark Manager and the File
    // Manager on Chrome OS.
    "chrome-extension",

    // NTP on Android.
    "chrome-native",

    // NTP on other platforms.
    "chrome-search",
};

#if BUILDFLAG(IS_IOS)
// The two schemes used on iOS for the NTP.
constexpr char kIosNtpAboutScheme[] = "about";
constexpr char kIosNtpChromeScheme[] = "chrome";
// The host string used on iOS for the NTP.
constexpr char kIosNtpHost[] = "newtab";
#endif

// Returns a blocklist based on the given |block| and |allow| pattern lists.
std::unique_ptr<URLBlocklist> BuildBlocklist(const base::Value::List* block,
                                             const base::Value::List* allow) {
  auto blocklist = std::make_unique<URLBlocklist>();
  if (block)
    blocklist->Block(*block);
  if (allow)
    blocklist->Allow(*allow);
  return blocklist;
}

const base::Value::List* GetPrefList(const PrefService* pref_service,
                                     std::optional<std::string> pref_path) {
  DCHECK(pref_service);

  if (!pref_path)
    return nullptr;

  DCHECK(!pref_path->empty());

  return pref_service->HasPrefPath(*pref_path)
             ? &pref_service->GetList(*pref_path)
             : nullptr;
}

bool BypassBlocklistWildcardForURL(const GURL& url) {
  const std::string& scheme = url.scheme();
  for (const char* bypass_scheme : kBypassBlocklistWildcardForSchemes) {
    if (scheme == bypass_scheme)
      return true;
  }
#if BUILDFLAG(IS_IOS)
  // Compare the chrome scheme and host against the chrome://newtab version of
  // the NTP URL.
  if (scheme == kIosNtpChromeScheme && url.host() == kIosNtpHost) {
    return true;
  }
  // Compare the URL scheme and path to the about:newtab version of the NTP URL.
  // Leading and trailing slashes must be removed because the host name is
  // parsed as the URL path (which may contain slashes).
  const std::string_view trimmed_path =
      base::TrimString(url.path_piece(), "/", base::TrimPositions::TRIM_ALL);
  if (scheme == kIosNtpAboutScheme && trimmed_path == kIosNtpHost) {
    return true;
  }
#endif
  return false;
}

bool IsWildcardBlocklist(const FilterComponents& filter) {
  return !filter.allow && filter.IsWildcard();
}

// Determines if the left-hand side `lhs` filter takes precedence over the
// right-hand side `rhs` filter. Returns true if `lhs` takes precedence over
// `rhs`, false otherwise.
bool FilterTakesPrecedence(const FilterComponents& lhs,
                           const FilterComponents& rhs) {
  // The "*" wildcard in the blocklist is the lowest priority filter.
  if (IsWildcardBlocklist(rhs)) {
    return true;
  }

  if (IsWildcardBlocklist(lhs)) {
    return false;
  }

  if (lhs.match_subdomains && !rhs.match_subdomains) {
    return false;
  }
  if (!lhs.match_subdomains && rhs.match_subdomains) {
    return true;
  }

  const size_t host_length = lhs.host.length();
  const size_t other_host_length = rhs.host.length();
  if (host_length != other_host_length) {
    return host_length > other_host_length;
  }

  const size_t path_length = lhs.path.length();
  const size_t other_path_length = rhs.path.length();
  if (path_length != other_path_length) {
    return path_length > other_path_length;
  }

  if (lhs.number_of_url_matching_conditions !=
      rhs.number_of_url_matching_conditions) {
    return lhs.number_of_url_matching_conditions >
           rhs.number_of_url_matching_conditions;
  }

  if (lhs.allow && !rhs.allow) {
    return true;
  }

  return false;
}

}  // namespace

// BlocklistSource implementation that blocks URLs, domains and schemes
// specified by the preference  `blocklist_pref_path`. The `allowlist_pref_path`
// preference specifies exceptions to the blocklist.
// Note that this implementation only supports one observer at a time. Adding a
// new observer will remove the previous one.
class DefaultBlocklistSource : public BlocklistSource {
 public:
  DefaultBlocklistSource(PrefService* pref_service,
                         std::optional<std::string> blocklist_pref_path,
                         std::optional<std::string> allowlist_pref_path)
      : blocklist_pref_path_(blocklist_pref_path),
        allowlist_pref_path_(allowlist_pref_path) {
    pref_change_registrar_.Init(pref_service);
  }
  DefaultBlocklistSource(const DefaultBlocklistSource&) = delete;
  DefaultBlocklistSource& operator=(const DefaultBlocklistSource&) = delete;
  ~DefaultBlocklistSource() override = default;

  const base::Value::List* GetBlocklistSpec() const override {
    return GetPrefList(pref_change_registrar_.prefs(), blocklist_pref_path_);
  }

  const base::Value::List* GetAllowlistSpec() const override {
    return GetPrefList(pref_change_registrar_.prefs(), allowlist_pref_path_);
  }

  // Adds an observer which will be notified when the blocklist is updated, i.e.
  // when the preferences `blocklist_pref_path_` and/or `allowlist_pref_path_`
  // have new values. If an observer already exists, it will be removed.
  void SetBlocklistObserver(base::RepeatingClosure observer) override {
    pref_change_registrar_.RemoveAll();
    if (blocklist_pref_path_) {
      pref_change_registrar_.Add(*blocklist_pref_path_, observer);
    }
    if (allowlist_pref_path_) {
      pref_change_registrar_.Add(*allowlist_pref_path_, observer);
    }
  }

 private:
  std::optional<std::string> blocklist_pref_path_;
  std::optional<std::string> allowlist_pref_path_;
  PrefChangeRegistrar pref_change_registrar_;
};

URLBlocklist::URLBlocklist() : url_matcher_(new URLMatcher) {}

URLBlocklist::~URLBlocklist() = default;

void URLBlocklist::Block(const base::Value::List& filters) {
  url_matcher::util::AddFilters(url_matcher_.get(), /*allow=*/false, &id_,
                                filters, &filters_);
}

void URLBlocklist::Allow(const base::Value::List& filters) {
  url_matcher::util::AddFilters(url_matcher_.get(), /*allow=*/true, &id_,
                                filters, &filters_);
}

bool URLBlocklist::IsURLBlocked(const GURL& url) const {
  return URLBlocklist::GetURLBlocklistState(url) ==
         URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

URLBlocklist::URLBlocklistState URLBlocklist::GetURLBlocklistState(
    const GURL& url) const {
  const FilterComponents* highest_priority_filter =
      GetHighestPriorityFilterFor(url);
  // Default neutral.
  if (!highest_priority_filter) {
    return URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE;
  }

  // Some of the internal Chrome URLs are not affected by the "*" in the
  // blocklist. Note that the "*" is the lowest priority filter possible, so
  // any higher priority filter will be applied first.
  if (IsWildcardBlocklist(*highest_priority_filter) &&
      BypassBlocklistWildcardForURL(url)) {
    return URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  }

  return highest_priority_filter->allow
             ? URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST
             : URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

const FilterComponents* URLBlocklist::GetHighestPriorityFilterFor(
    const GURL& url) const {
  const FilterComponents* highest_priority_filter = nullptr;
  for (const auto& pattern_id : url_matcher_->MatchURL(url)) {
    const auto it = filters_.find(pattern_id);
    CHECK(it != filters_.end(), base::NotFatalUntil::M130);
    const FilterComponents& filter = it->second;
    if (!highest_priority_filter ||
        FilterTakesPrecedence(filter, *highest_priority_filter)) {
      highest_priority_filter = &filter;
    }
  }
  return highest_priority_filter;
}

URLBlocklistManager::URLBlocklistManager(
    PrefService* pref_service,
    std::optional<std::string> blocklist_pref_path,
    std::optional<std::string> allowlist_pref_path)
    : blocklist_(new URLBlocklist) {
  DCHECK(blocklist_pref_path || allowlist_pref_path);

  // This class assumes that it is created on the same thread that
  // |pref_service_| lives on.
  ui_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});

  default_blocklist_source_ = std::make_unique<DefaultBlocklistSource>(
      pref_service, blocklist_pref_path, allowlist_pref_path);

  default_blocklist_source_->SetBlocklistObserver(base::BindRepeating(
      &URLBlocklistManager::ScheduleUpdate, base::Unretained(this)));
  // Start enforcing the policies without a delay when they are present at
  // startup.
  const base::Value::List* block =
      default_blocklist_source_->GetBlocklistSpec();
  const base::Value::List* allow =
      default_blocklist_source_->GetAllowlistSpec();
  if (block || allow)
    SetBlocklist(BuildBlocklist(block, allow));
}

URLBlocklistManager::~URLBlocklistManager() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
}

void URLBlocklistManager::ScheduleUpdate() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Cancel pending updates, if any. This can happen if two preferences that
  // change the blocklist are updated in one message loop cycle. In those cases,
  // only rebuild the blocklist after all the preference updates are processed.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&URLBlocklistManager::Update,
                                           ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlocklistManager::Update() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  // The URLBlocklist is built in the background. Once it's ready, it is passed
  // to the URLBlocklistManager back on ui_task_runner_.

  const BlocklistSource* current_source = override_blocklist_source_
                                              ? override_blocklist_source_.get()
                                              : default_blocklist_source_.get();

  const base::Value::List* block = current_source->GetBlocklistSpec();
  const base::Value::List* allow = current_source->GetAllowlistSpec();

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &BuildBlocklist,
          base::Owned(block
                          ? std::make_unique<base::Value::List>(block->Clone())
                          : nullptr),
          base::Owned(allow
                          ? std::make_unique<base::Value::List>(allow->Clone())
                          : nullptr)),
      base::BindOnce(&URLBlocklistManager::SetBlocklist,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlocklistManager::SetBlocklist(
    std::unique_ptr<URLBlocklist> blocklist) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  blocklist_ = std::move(blocklist);
}

bool URLBlocklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Ignore blob scheme for two reasons:
  // 1) Its used to deliver the response to the renderer.
  // 2) A page on the allowlist can use blob URLs internally.
  return !url.SchemeIs(url::kBlobScheme) && blocklist_->IsURLBlocked(url);
}

URLBlocklist::URLBlocklistState URLBlocklistManager::GetURLBlocklistState(
    const GURL& url) const {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  return blocklist_->GetURLBlocklistState(url);
}

// static
void URLBlocklistManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(policy_prefs::kUrlBlocklist);
  registry->RegisterListPref(policy_prefs::kUrlAllowlist);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist);
#endif
  registry->RegisterIntegerPref(
      policy_prefs::kSafeSitesFilterBehavior,
      static_cast<int>(SafeSitesFilterBehavior::kSafeSitesFilterDisabled));
}

void URLBlocklistManager::SetOverrideBlockListSource(
    std::unique_ptr<BlocklistSource> blocklist_source) {
  if (blocklist_source) {
    override_blocklist_source_ = std::move(blocklist_source);
    override_blocklist_source_->SetBlocklistObserver(base::BindRepeating(
        &URLBlocklistManager::ScheduleUpdate, base::Unretained(this)));
  } else {
    override_blocklist_source_.reset();
  }
  ScheduleUpdate();
}

}  // namespace policy
