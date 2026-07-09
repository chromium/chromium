// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_local_database_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_database.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_update_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v5_update_protocol_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// TODO(crbug.com/362791941): Handle v4 references
// TODO(crbug.com/362791941): Convert |comments| to `comments`
// TODO(crbug.com/362791941): Change DCHECKs to CHECKs
namespace safe_browsing {

namespace {

struct CommandLineSwitchAndThreatType {
  const char* cmdline_switch;
  ThreatType threat_type;
};

// The expiration time of the full hash stored in the artificial database.
const int64_t kFullHashExpiryTimeInMinutes = 60;

// The number of bytes in a full hash entry.
const int64_t kBytesPerFullHashEntry = 32;

// The minimum number of entries in the allowlist. If the actual size is
// smaller than this number, the allowlist is considered as unavailable.
const int kHighConfidenceAllowlistMinimumEntryCount = 100;

const ThreatSeverity kLeastSeverity =
    std::numeric_limits<ThreatSeverity>::max();

ListInfos GetListInfos() {
  using enum SBThreatType;

  // NOTE(vakh): When adding a store here, add the corresponding store-specific
  // histograms also.
  // The first argument to ListInfo specifies whether to sync hash prefixes for
  // that list. This can be false for two reasons:
  // - The server doesn't support that list yet. Once the server adds support
  //   for it, it can be changed to true.
  // - The list doesn't have hash prefixes to match. All requests lead to full
  //   hash checks. For instance: GetChromeUrlApiId()

#if BUILDFLAG(IS_IOS)
  const bool kSyncOnIos = true;
#else
  const bool kSyncOnIos = false;
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool kIsChromeBranded = true;
#else
  const bool kIsChromeBranded = false;
#endif

  const bool kSyncOnDesktopBuilds = !kSyncOnIos;
  const bool kSyncOnChromeDesktopBuilds =
      kIsChromeBranded && kSyncOnDesktopBuilds;
  const bool kSyncAlways = true;
  const bool kSyncNever = false;

  return ListInfos({
      ListInfo(kSyncAlways, "UrlSoceng", GetUrlSocEngId(),
               SB_THREAT_TYPE_URL_PHISHING),
      ListInfo(kSyncAlways, "UrlMalware", GetUrlMalwareId(),
               SB_THREAT_TYPE_URL_MALWARE),
      ListInfo(kSyncAlways, "UrlUws", GetUrlUwsId(),
               SB_THREAT_TYPE_URL_UNWANTED),
      ListInfo(kSyncOnDesktopBuilds, "UrlMalBin", GetUrlMalBinId(),
               SB_THREAT_TYPE_URL_BINARY_MALWARE),
      ListInfo(kSyncOnDesktopBuilds, "ChromeExtMalware",
               GetChromeExtMalwareId(), SB_THREAT_TYPE_EXTENSION),
      ListInfo(kSyncAlways, "UrlBilling", GetUrlBillingId(),
               SB_THREAT_TYPE_BILLING),
      ListInfo(kSyncOnDesktopBuilds, "UrlCsdDownloadAllowlist",
               GetUrlCsdDownloadAllowlistId(),
               SB_THREAT_TYPE_CSD_DOWNLOAD_ALLOWLIST),
      ListInfo(kSyncOnChromeDesktopBuilds || kSyncOnIos, "UrlCsdAllowlist",
               GetUrlCsdAllowlistId(), SB_THREAT_TYPE_CSD_ALLOWLIST),
      ListInfo(kSyncOnChromeDesktopBuilds, "UrlSubresourceFilter",
               GetUrlSubresourceFilterId(), SB_THREAT_TYPE_SUBRESOURCE_FILTER),
      ListInfo(kSyncOnChromeDesktopBuilds, "UrlSuspiciousSite",
               GetUrlSuspiciousSiteId(), SB_THREAT_TYPE_SUSPICIOUS_SITE),
      ListInfo(kSyncNever, "", GetChromeUrlApiId(), SB_THREAT_TYPE_API_ABUSE),
      ListInfo(kSyncOnChromeDesktopBuilds || kSyncOnIos,
               "UrlHighConfidenceAllowlist", GetUrlHighConfidenceAllowlistId(),
               SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST),
  });
  // NOTE(vakh): IMPORTANT: Please make sure that the server already supports
  // any list before adding it to this list otherwise the prefix updates break
  // for all Canary users.
}

base::span<const CommandLineSwitchAndThreatType> GetSwitchAndThreatTypes() {
  static constexpr CommandLineSwitchAndThreatType
      kCommandLineSwitchAndThreatType[] = {
          {switches::kMarkAsPasswordProtectionAllowlisted, CSD_ALLOWLIST},
          {switches::kMarkAsHighConfidenceAllowlisted,
           HIGH_CONFIDENCE_ALLOWLIST},
          {switches::kMarkAsPhishing, SOCIAL_ENGINEERING},
          {switches::kMarkAsMalware, MALWARE_THREAT},
          {switches::kMarkAsUws, UNWANTED_SOFTWARE}};
  return kCommandLineSwitchAndThreatType;
}

// Returns the severity information about a given SafeBrowsing list. The lowest
// value is 0, which represents the most severe list.
ThreatSeverity GetThreatSeverity(const ListIdentifier& list_id) {
  switch (list_id.threat_type()) {
    case MALWARE_THREAT:
    case SOCIAL_ENGINEERING:
    case MALICIOUS_BINARY:
      return 0;
    case UNWANTED_SOFTWARE:
      return 1;
    case API_ABUSE:
    case SUBRESOURCE_FILTER:
      return 2;
    case CSD_ALLOWLIST:
    case HIGH_CONFIDENCE_ALLOWLIST:
      return 3;
    case SUSPICIOUS:
      return 4;
    case BILLING:
      return 15;
    case CLIENT_INCIDENT:
    case CSD_DOWNLOAD_ALLOWLIST:
    case POTENTIALLY_HARMFUL_APPLICATION:
    case SOCIAL_ENGINEERING_PUBLIC:
    case THREAT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected ThreatType encountered: "
                   << list_id.threat_type();
  }
}

// This is only valid for types that are passed to GetBrowseUrl().
ListIdentifier GetUrlIdFromSBThreatType(SBThreatType sb_threat_type) {
  using enum SBThreatType;

  switch (sb_threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
      return GetUrlMalwareId();

    case SB_THREAT_TYPE_URL_PHISHING:
      return GetUrlSocEngId();

    case SB_THREAT_TYPE_URL_UNWANTED:
      return GetUrlUwsId();

    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return GetUrlSuspiciousSiteId();

    case SB_THREAT_TYPE_BILLING:
      return GetUrlBillingId();

    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    case SB_THREAT_TYPE_EXTENSION:
    case DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case SB_THREAT_TYPE_AD_SAMPLE:
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case SB_THREAT_TYPE_APK_DOWNLOAD:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
    case SB_THREAT_TYPE_CSD_DOWNLOAD_ALLOWLIST:
    case SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE:
      NOTREACHED();
  }
}

StoresToCheck CreateStoresToCheckFromSBThreatTypeSet(
    const SBThreatTypeSet& threat_types) {
  StoresToCheck stores_to_check;
  for (SBThreatType sb_threat_type : threat_types) {
    stores_to_check.insert(GetUrlIdFromSBThreatType(sb_threat_type));
  }
  return stores_to_check;
}

void RecordTimeSinceLastUpdateHistograms(const base::Time& last_response_time) {
  if (last_response_time.is_null()) {
    return;
  }

  base::TimeDelta time_since_update = base::Time::Now() - last_response_time;
  UMA_HISTOGRAM_LONG_TIMES_100(
      "SafeBrowsing.V4LocalDatabaseManager.TimeSinceLastUpdateResponse",
      time_since_update);
}

void HandleUrlCallback(base::OnceCallback<void(bool)> callback,
                       DbLookupResult lookup_result) {
  bool allowed = !lookup_result.results.empty();
  // This callback was already run asynchronously so no need for another
  // thread hop.
  std::move(callback).Run(allowed);
}

void OnCheckForUrlHighConfidenceAllowlistComplete(
    SafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlistCallback
        callback,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details,
    bool url_on_high_confidence_allowlist) {
  std::move(callback).Run(url_on_high_confidence_allowlist,
                          std::move(logging_details));
}

}  // namespace

SBLocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::vector<GURL>& urls,
    std::optional<bool> needs_full_hash_check_after_local_match)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SBThreatType::SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check),
      urls(urls),
      needs_full_hash_check_after_local_match(
          needs_full_hash_check_after_local_match) {
  for (const auto& url : urls) {
    SBProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  }
  full_hash_threat_types.assign(full_hashes.size(),
                                SBThreatType::SB_THREAT_TYPE_SAFE);
}

SBLocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::set<FullHashStr>& full_hashes_set,
    std::optional<bool> needs_full_hash_check_after_local_match)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SBThreatType::SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check),
      needs_full_hash_check_after_local_match(
          needs_full_hash_check_after_local_match) {
  full_hashes.assign(full_hashes_set.begin(), full_hashes_set.end());
  DCHECK(full_hashes.size());
  full_hash_threat_types.assign(full_hashes.size(),
                                SBThreatType::SB_THREAT_TYPE_SAFE);
}

SBLocalDatabaseManager::PendingCheck::~PendingCheck() {
  DCHECK(!is_in_pending_checks);
}

void SBLocalDatabaseManager::PendingCheck::Abandon() {
  client = nullptr;
}

// static
const SBLocalDatabaseManager*
    SBLocalDatabaseManager::current_local_database_manager_;

// static
scoped_refptr<SBLocalDatabaseManager> SBLocalDatabaseManager::Create(
    const base::FilePath& base_path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    ExtendedReportingLevelCallback extended_reporting_level_callback) {
  return base::WrapRefCounted(new SBLocalDatabaseManager(
      base_path, extended_reporting_level_callback, std::move(ui_task_runner),
      std::move(io_task_runner), nullptr));
}

void SBLocalDatabaseManager::CollectDatabaseManagerInfo(
    DatabaseManagerInfo* database_manager_info,
    FullHashCacheInfo* full_hash_cache_info) const {
  if (update_protocol_manager_) {
    update_protocol_manager_->CollectUpdateInfo(
        database_manager_info->mutable_update_info());
  }
  if (sb_database_) {
    sb_database_->CollectDatabaseInfo(
        database_manager_info->mutable_database_info());
  }
  if (v4_get_hash_protocol_manager_) {
    v4_get_hash_protocol_manager_->CollectFullHashCacheInfo(
        full_hash_cache_info);
  }
}

SBLocalDatabaseManager::SBLocalDatabaseManager(
    const base::FilePath& base_path,
    ExtendedReportingLevelCallback extended_reporting_level_callback,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_tests)
    : SafeBrowsingDatabaseManager(std::move(ui_task_runner)),
      base_path_(base_path),
      extended_reporting_level_callback_(extended_reporting_level_callback),
      list_infos_(GetListInfos()),
      task_runner_(task_runner_for_tests
                       ? task_runner_for_tests
                       : base::ThreadPool::CreateSequencedTaskRunner(
                             {base::MayBlock(),
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      sb_database_(std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(nullptr))),
      enabled_(false),
      is_shutdown_(false) {
  DCHECK(this->ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());
}

SBLocalDatabaseManager::~SBLocalDatabaseManager() {
  DCHECK(!enabled_);
}

//
// Start: SafeBrowsingDatabaseManager implementation
//

void SBLocalDatabaseManager::CancelCheck(Client* client) {
  if (is_shutdown_) {
    // In the shutdown case, we have already dropped queued and pending checks
    // in `DropQueuedAndPendingChecks()`, so there is no work needed here.
    return;
  }
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  // We can't use IsDatabaseReady() here because there's several expected cases
  // where a client could cancel while the request is still queued (e.g.
  // timeouts, tab being closed).
  DCHECK(enabled_);
  auto pending_it =
      std::ranges::find(pending_checks_, client, &PendingCheck::client);
  if (pending_it != pending_checks_.end()) {
    (*pending_it)->Abandon();
    RemovePendingCheck(pending_it);
  }

  auto queued_it =
      std::ranges::find(queued_checks_, client, &PendingCheck::client);
  if (queued_it != queued_checks_.end()) {
    queued_checks_.erase(queued_it);
  }
}

bool SBLocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}

bool SBLocalDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    CheckBrowseUrlType check_type) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!threat_types.empty());
  DCHECK(SBThreatTypeSetIsValidForCheckBrowseUrl(threat_types));
  DCHECK(check_type == CheckBrowseUrlType::kHashDatabase)
      << "SB Local database only supports hash database check.";

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_BROWSE_URL,
      CreateStoresToCheckFromSBThreatTypeSet(threat_types),
      std::vector<GURL>(1, url),
      /*needs_full_hash_check_after_local_match=*/true);

  HandleCheck(std::move(check));
  RecordTimeSinceLastUpdateHistograms(
      update_protocol_manager_->last_response_time());
  return false;
}

bool SBLocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_ || url_chain.empty()) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_DOWNLOAD_URLS,
      StoresToCheck({GetUrlMalBinId()}), url_chain,
      /*needs_full_hash_check_after_local_match=*/true);

  HandleCheck(std::move(check));
  return false;
}

bool SBLocalDatabaseManager::CheckExtensionIDs(
    const std::set<FullHashStr>& extension_ids,
    Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_EXTENSION_IDS,
      StoresToCheck({GetChromeExtMalwareId()}), extension_ids,
      /*needs_full_hash_check_after_local_match=*/
      !base::FeatureList::IsEnabled(kExtensionBlocklistSkipNetworkQuery));

  HandleCheck(std::move(check));
  return false;
}

void SBLocalDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    CheckUrlForHighConfidenceAllowlistCallback callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSkipHighConfidenceAllowlist)) {
    ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*url_on_high_confidence_allowlist=*/false,
                                  /*logging_details=*/std::nullopt));
    return;
  }

  StoresToCheck stores_to_check({GetUrlHighConfidenceAllowlistId()});
  bool all_stores_available = AreAllStoresAvailableNow(stores_to_check);
  bool is_artificial_prefix_empty =
      artificially_marked_store_and_hash_prefixes_.empty();
  bool is_allowlist_too_small =
      IsStoreTooSmall(GetUrlHighConfidenceAllowlistId(), kBytesPerFullHashEntry,
                      kHighConfidenceAllowlistMinimumEntryCount);
  HighConfidenceAllowlistCheckLoggingDetails logging_details;
  logging_details.were_all_stores_available = all_stores_available;
  logging_details.was_allowlist_size_too_small = is_allowlist_too_small;
  if (!IsDatabaseReady() ||
      (is_allowlist_too_small && is_artificial_prefix_empty) ||
      !CanCheckUrl(url) ||
      (!all_stores_available && is_artificial_prefix_empty)) {
    // NOTE(vakh): If Safe Browsing isn't enabled yet, or if the URL isn't a
    // navigation URL, or if the allowlist isn't ready yet, or if the allowlist
    // is too small, return that there is a match. The full URL check won't be
    // performed, but hash-based check will still be done. If any artificial
    // matches are present, consider the allowlist as ready.
    ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*url_on_high_confidence_allowlist=*/true,
                                  std::move(logging_details)));
    return;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url),
      /*needs_full_hash_check_after_local_match=*/std::nullopt);

  HandleAllowlistCheck(
      std::move(check), /*allow_async_full_hash_check=*/false,
      base::BindOnce(&OnCheckForUrlHighConfidenceAllowlistComplete,
                     std::move(callback), std::move(logging_details)));
}

bool SBLocalDatabaseManager::CheckUrlForSubresourceFilter(const GURL& url,
                                                          Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check(
      {GetUrlSocEngId(), GetUrlSubresourceFilterId()});
  if (!AreAnyStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER,
      stores_to_check, std::vector<GURL>(1, url),
      /*needs_full_hash_check_after_local_match=*/true);

  HandleCheck(std::move(check));
  return false;
}

AsyncMatch SBLocalDatabaseManager::CheckCsdAllowlistUrl(const GURL& url,
                                                        Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check({GetUrlCsdAllowlistId()});
  // If any artificial matches are present, consider the allowlist as ready.
  bool is_artificial_prefix_empty =
      artificially_marked_store_and_hash_prefixes_.empty();
  if ((!AreAllStoresAvailableNow(stores_to_check) &&
       is_artificial_prefix_empty) ||
      !CanCheckUrl(url)) {
    // Fail open: Allowlist everything. Otherwise we may run the
    // CSD phishing/malware detector on popular domains and generate
    // undue load on the client and server, or send Password Reputation
    // requests on popular sites. This has the effect of disabling
    // CSD phishing/malware detection and password reputation service
    // until the store is first synced and/or loaded from disk.
    return AsyncMatch::MATCH;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_CSD_ALLOWLIST, stores_to_check,
      std::vector<GURL>(1, url),
      /*needs_full_hash_check_after_local_match=*/true);

  HandleAllowlistCheck(std::move(check),
                       /*allow_async_full_hash_check=*/true,
                       base::OnceCallback<void(bool)>());
  return AsyncMatch::ASYNC;
}

void SBLocalDatabaseManager::MatchDownloadAllowlistUrl(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check({GetUrlCsdDownloadAllowlistId()});

  if (!AreAllStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    // Fail close: Allowlist nothing. This may generate download-protection
    // pings for allowlisted domains, but that's fine.
    ui_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
    return;
  }

  HandleUrl(url, stores_to_check, std::move(callback));
}

ThreatSource SBLocalDatabaseManager::GetBrowseUrlThreatSource(
    CheckBrowseUrlType check_type) const {
  DCHECK(check_type == CheckBrowseUrlType::kHashDatabase)
      << "SB Local database only supports hash database check.";
  return ThreatSource::LOCAL_PVER4;
}

ThreatSource SBLocalDatabaseManager::GetNonBrowseUrlThreatSource() const {
  return ThreatSource::LOCAL_PVER4;
}

void SBLocalDatabaseManager::StartOnUIThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnUIThread(url_loader_factory, config);

  db_updated_callback_ = base::BindRepeating(
      &SBLocalDatabaseManager::DatabaseUpdated, weak_factory_.GetWeakPtr());

  SetupUpdateProtocolManager(url_loader_factory, config);
  SetupDatabase();

  enabled_ = true;
  is_shutdown_ = false;

  current_local_database_manager_ = this;
}

void SBLocalDatabaseManager::StopOnUIThread(bool shutdown) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  enabled_ = false;
  is_shutdown_ = shutdown;

  current_local_database_manager_ = nullptr;

  // On shutdown, it's acceptable to fail to respond.
  if (shutdown) {
    DropQueuedAndPendingChecks();
  } else {
    RespondSafeToQueuedAndPendingChecks();
  }

  // Delete the SBDatabase. Any pending writes to disk are completed.
  // This operation happens on the task_runner on which sb_database_ operates
  // and doesn't block the IO thread.
  if (sb_database_) {
    sb_database_->StopOnUIThread();
  }
  sb_database_.reset();

  // Delete the SBUpdateProtocolManager.
  // This cancels any in-flight update request.
  update_protocol_manager_.reset();

  db_updated_callback_.Reset();

  weak_factory_.InvalidateWeakPtrs();

  SafeBrowsingDatabaseManager::StopOnUIThread(shutdown);
}

bool SBLocalDatabaseManager::IsDatabaseReady() const {
  return enabled_ && !!sb_database_;
}

//
// End: SafeBrowsingDatabaseManager implementation
//

void SBLocalDatabaseManager::DatabaseReadyForChecks(
    base::Time start_time,
    std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter> sb_database) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  base::TimeDelta delta = base::Time::Now() - start_time;
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::UmaHistogramTimes("SafeBrowsing.V5DatabaseInitializationTime", delta);
  } else {
    base::UmaHistogramTimes("SafeBrowsing.V4DatabaseInitializationTime", delta);
  }
  base::UmaHistogramTimes("SafeBrowsing.SBDatabaseInitializationTime", delta);

  sb_database->InitializeOnUIThread();

  // The following check is needed because it is possible that by the time the
  // database is ready, StopOnUIThread has been called.
  if (enabled_) {
    sb_database_ = std::move(sb_database);

    sb_database_->RecordFileSizeHistograms();

    PopulateArtificialDatabase();

    // The consistency of the stores read from the disk needs to verified. Post
    // that task on the task runner. It calls |DatabaseReadyForUpdates|
    // callback with the stores to reset, if any, and then we can schedule the
    // database updates.
    sb_database_->VerifyChecksum(
        base::BindOnce(&SBLocalDatabaseManager::DatabaseReadyForUpdates,
                       weak_factory_.GetWeakPtr()));

    ProcessQueuedChecks();
  } else {
    // Schedule the deletion of sb_database off IO thread.
    sb_database.reset();
  }
}

void SBLocalDatabaseManager::DatabaseReadyForUpdates(
    const std::vector<ListIdentifier>& stores_to_reset) {
  if (IsDatabaseReady()) {
    sb_database_->ResetStores(stores_to_reset);
    UpdateListClientStates(GetStoreStateMap());

    // The database is ready to process updates. Schedule them now.
    update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());
  }
}

void SBLocalDatabaseManager::DatabaseUpdated() {
  if (IsDatabaseReady()) {
    update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());

    sb_database_->RecordFileSizeHistograms();
    UpdateListClientStates(GetStoreStateMap());

    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished, this));
  }
}

void SBLocalDatabaseManager::GetArtificialPrefixMatches(
    const std::unique_ptr<PendingCheck>& check) {
  if (artificially_marked_store_and_hash_prefixes_.empty()) {
    return;
  }
  for (const auto& full_hash : check->full_hashes) {
    for (const StoreAndHashPrefix& artificial_store_and_hash_prefix :
         artificially_marked_store_and_hash_prefixes_) {
      FullHashStr artificial_full_hash =
          artificial_store_and_hash_prefix.hash_prefix;
      DCHECK_EQ(crypto::kSHA256Length, artificial_full_hash.size());
      if (artificial_full_hash == full_hash &&
          check->stores_to_check.contains(
              artificial_store_and_hash_prefix.list_id)) {
        (check->artificial_full_hash_to_store_and_hash_prefixes)[full_hash] = {
            artificial_store_and_hash_prefix};
      }
    }
  }
}

void SBLocalDatabaseManager::GetPrefixMatches(
    PendingCheck* check,
    base::OnceCallback<void(DbLookupResult)> callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(IsDatabaseReady());

  check->local_db_lookup_start_time = base::TimeTicks::Now();
  sb_database_->GetStoresMatchingFullHash(
      check->full_hashes, check->stores_to_check, std::move(callback));
}

void SBLocalDatabaseManager::GetSeverestThreatTypeAndMetadata(
    const std::vector<FullHashInfo>& full_hash_infos,
    const std::vector<FullHashStr>& full_hashes,
    std::vector<SBThreatType>* full_hash_threat_types,
    SBThreatType* most_severe_threat_type,
    ThreatMetadata* metadata) {
  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.V4LocalDatabaseManager.ThreatInfoSize",
                           full_hash_infos.size());
  ThreatSeverity most_severe_yet = kLeastSeverity;
  for (const FullHashInfo& fhi : full_hash_infos) {
    ThreatSeverity severity = GetThreatSeverity(fhi.list_id);
    SBThreatType threat_type = GetSBThreatTypeForList(fhi.list_id);

    const auto& it = std::ranges::find(full_hashes, fhi.full_hash);
    CHECK(it != full_hashes.end());
    (*full_hash_threat_types)[it - full_hashes.begin()] = threat_type;

    if (severity < most_severe_yet) {
      most_severe_yet = severity;
      *most_severe_threat_type = threat_type;
      *metadata = fhi.metadata;
    }
  }
}

StoresToCheck SBLocalDatabaseManager::GetStoresForFullHashRequests() {
  StoresToCheck stores_for_full_hash;
  for (const auto& info : list_infos_) {
    stores_for_full_hash.insert(info.list_id());
  }
  return stores_for_full_hash;
}

std::unique_ptr<StoreStateMap> SBLocalDatabaseManager::GetStoreStateMap() {
  return sb_database_->GetStoreStateMap();
}

// Returns the SBThreatType corresponding to a given SafeBrowsing list.
SBThreatType SBLocalDatabaseManager::GetSBThreatTypeForList(
    const ListIdentifier& list_id) {
  auto it = std::ranges::find(list_infos_, list_id, &ListInfo::list_id);
  CHECK(list_infos_.end() != it);
  DCHECK_NE(SBThreatType::SB_THREAT_TYPE_SAFE, it->sb_threat_type());
  DCHECK_NE(SBThreatType::SB_THREAT_TYPE_UNUSED, it->sb_threat_type());
  return it->sb_threat_type();
}

void SBLocalDatabaseManager::HandleAllowlistCheck(
    std::unique_ptr<PendingCheck> check,
    bool allow_async_full_hash_check,
    base::OnceCallback<void(bool)> callback) {
  // We don't bother queuing allowlist checks since the DB will
  // normally be available already -- allowlists are used after page load,
  // and navigations are blocked until the DB is ready and dequeues checks.
  // The caller should have already checked that the DB is ready.
  DCHECK(sb_database_);

  PendingCheck* check_ptr = check.get();

  if (!callback.is_null()) {
    // If StopOnUIThread is called weak_factory_ will get invalidated and
    // HandleAllowlistCheckContinuation won't be called. We still want to run
    // the callback though. See comment in CheckUrlForHighConfidenceAllowlist
    // on why this returns true.
    callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), true);
  }

  GetPrefixMatches(
      check_ptr,
      base::BindOnce(&SBLocalDatabaseManager::HandleAllowlistCheckContinuation,
                     weak_factory_.GetWeakPtr(), std::move(check),
                     allow_async_full_hash_check, std::move(callback)));

  AddPendingCheck(check_ptr);
}

void SBLocalDatabaseManager::HandleAllowlistCheckContinuation(
    std::unique_ptr<PendingCheck> check,
    bool allow_async_full_hash_check,
    base::OnceCallback<void(bool)> callback,
    DbLookupResult lookup_result) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  AsyncMatch local_match;
    if (!IsDatabaseReady()) {
      DCHECK(pending_checks_.empty());
      return;
    }

    const auto it = pending_checks_.find(check.get());
    if (it == pending_checks_.end()) {
      // The check has since been cancelled.
      return;
    }

    RemovePendingCheck(it);

  check->full_hash_to_store_and_hash_prefixes = lookup_result.results;
  GetArtificialPrefixMatches(check);
  if (check->full_hash_to_store_and_hash_prefixes.empty() &&
      check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    local_match = AsyncMatch::NO_MATCH;
  } else {
    // Look for any full-length hash in the matches. If there is one,
    // there's no need for a full-hash check. This saves bandwidth for
    // very popular sites since they'll have full-length hashes locally.
    // These loops will have exactly 1 entry most of the time.
    bool found = false;
    for (const auto& entry : check->full_hash_to_store_and_hash_prefixes) {
      for (const auto& store_and_prefix : entry.second) {
        if (store_and_prefix.hash_prefix.size() == kMaxHashPrefixLength) {
          local_match = AsyncMatch::MATCH;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      if (!allow_async_full_hash_check) {
        local_match = AsyncMatch::NO_MATCH;
      } else {
        local_match = AsyncMatch::ASYNC;
        ScheduleFullHashCheck(std::move(check));
        return;
      }
    }
  }

  bool did_match_allowlist = local_match == AsyncMatch::MATCH;
  if (check->client_callback_type == ClientCallbackType::CHECK_OTHER) {
      // This is already asynchronous so no need for another PostTask.
      std::move(callback).Run(did_match_allowlist);
  } else if (check->client_callback_type ==
             ClientCallbackType::CHECK_CSD_ALLOWLIST) {
      check->most_severe_threat_type =
          did_match_allowlist ? SBThreatType::SB_THREAT_TYPE_CSD_ALLOWLIST
                              : SBThreatType::SB_THREAT_TYPE_SAFE;
      RespondToClient(std::move(check));
  } else {
    NOTREACHED();
  }
}

void SBLocalDatabaseManager::HandleCheck(std::unique_ptr<PendingCheck> check) {
  if (!sb_database_) {
    check->queue_start_time = base::TimeTicks::Now();
    queued_checks_.push_back(std::move(check));
    return;
  }

  PendingCheck* check_ptr = check.get();
  GetPrefixMatches(
      check_ptr,
      base::BindOnce(&SBLocalDatabaseManager::HandleCheckContinuation,
                     weak_factory_.GetWeakPtr(), std::move(check)));

  AddPendingCheck(check_ptr);
}

void SBLocalDatabaseManager::HandleCheckContinuation(
    std::unique_ptr<PendingCheck> check,
    DbLookupResult lookup_result) {
  if (!IsDatabaseReady()) {
    DCHECK(pending_checks_.empty());
    return;
  }

  base::UmaHistogramTimes(
      "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup",
      base::TimeTicks::Now() - check->local_db_lookup_start_time);

  base::UmaHistogramTimes(
      "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
      base::TimeTicks::Now() - lookup_result.db_thread_end_time);

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  RemovePendingCheck(it);

  if (check->client_callback_type == ClientCallbackType::CHECK_BROWSE_URL) {
    UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.CheckBrowseUrl.HasLocalMatch2",
                          !lookup_result.results.empty());
  }

  check->full_hash_to_store_and_hash_prefixes = lookup_result.results;
  GetArtificialPrefixMatches(check);
  RespondOrScheduleFullHashCheck(std::move(check));
}

void SBLocalDatabaseManager::PopulateArtificialDatabase() {
  for (const auto& switch_and_threat_type : GetSwitchAndThreatTypes()) {
    const std::string raw_artificial_urls =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switch_and_threat_type.cmdline_switch);
    base::StringTokenizer tokenizer(raw_artificial_urls, ",");
    while (tokenizer.GetNext()) {
      ListIdentifier artificial_list_id(GetCurrentPlatformType(), URL,
                                        switch_and_threat_type.threat_type);
      FullHashStr full_hash =
          SBProtocolManagerUtil::GetFullHash(GURL(tokenizer.token_piece()));
      artificially_marked_store_and_hash_prefixes_.emplace_back(
          artificial_list_id, full_hash);
    }
  }
}

void SBLocalDatabaseManager::ScheduleFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Add check to pending_checks_ before scheduling PerformFullHashCheck so that
  // even if the client calls CancelCheck before PerformFullHashCheck gets
  // called, the check can be found in pending_checks_.
  AddPendingCheck(check.get());

  // If the full hash matches one from the artificial list, don't send the
  // request to the server.
  if (!check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    std::vector<FullHashInfo> full_hash_infos;
    for (const auto& entry :
         check->artificial_full_hash_to_store_and_hash_prefixes) {
      for (const auto& store_and_prefix : entry.second) {
        ListIdentifier list_id = store_and_prefix.list_id;
        base::Time next =
            base::Time::Now() + base::Minutes(kFullHashExpiryTimeInMinutes);
        full_hash_infos.emplace_back(entry.first, list_id, next);
      }
    }

    ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&SBLocalDatabaseManager::OnFullHashResponse,
                                  weak_factory_.GetWeakPtr(), std::move(check),
                                  full_hash_infos));
  } else {
    // Post on the UI thread to enforce async behavior.
    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SBLocalDatabaseManager::PerformFullHashCheck,
                       weak_factory_.GetWeakPtr(), std::move(check)));
  }
}

void SBLocalDatabaseManager::HandleUrl(
    const GURL& url,
    const StoresToCheck& stores_to_check,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url),
      /*needs_full_hash_check_after_local_match=*/std::nullopt);

  GetPrefixMatches(check.get(),
                   base::BindOnce(&HandleUrlCallback, std::move(callback)));
}

void SBLocalDatabaseManager::OnFullHashResponse(
    std::unique_ptr<PendingCheck> check,
    const std::vector<FullHashInfo>& full_hash_infos) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!check->get_full_hash_request_start_time.is_null()) {
    base::UmaHistogramTimes(
        "SafeBrowsing.V4CheckUrl.TimeTaken.GetFullHashDuration",
        now - check->get_full_hash_request_start_time);
  }
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (!IsDatabaseReady()) {
    DCHECK(pending_checks_.empty());
    return;
  }

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  base::TimeTicks start_processing = base::TimeTicks::Now();
  // Find out the most severe threat, if any, to report to the client.
  GetSeverestThreatTypeAndMetadata(
      full_hash_infos, check->full_hashes, &check->full_hash_threat_types,
      &check->most_severe_threat_type, &check->url_metadata);
  base::UmaHistogramTimes(
      "SafeBrowsing.V4CheckUrl.TimeTaken.ResponseProcessingDuration",
      base::TimeTicks::Now() - start_processing);

  RemovePendingCheck(it);
  RespondToClient(std::move(check));
}

void SBLocalDatabaseManager::PerformFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  DCHECK(!check->full_hash_to_store_and_hash_prefixes.empty());

  // If the database isn't ready, the service has been turned off, so silently
  // drop the check.
  if (IsDatabaseReady()) {
    if (!check->get_full_hash_queue_start_time.is_null()) {
      base::UmaHistogramTimes(
          "SafeBrowsing.V4CheckUrl.TimeTaken.GetFullHashQueueDelay",
          base::TimeTicks::Now() - check->get_full_hash_queue_start_time);
    }
    check->get_full_hash_request_start_time = base::TimeTicks::Now();
    FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes =
        check->full_hash_to_store_and_hash_prefixes;
    v4_get_hash_protocol_manager_->GetFullHashes(
        full_hash_to_store_and_hash_prefixes, list_client_states_,
        base::BindOnce(&SBLocalDatabaseManager::OnFullHashResponse,
                       weak_factory_.GetWeakPtr(), std::move(check)));
  } else {
    DCHECK(pending_checks_.empty());
  }
}

void SBLocalDatabaseManager::ProcessQueuedChecks() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (auto& it : checks) {
    PendingCheck* check_ptr = it.get();

    if (!check_ptr->queue_start_time.is_null()) {
      base::UmaHistogramTimes(
          "SafeBrowsing.V4CheckUrl.TimeTaken.DatabaseNotReadyQueueDelay",
          base::TimeTicks::Now() - check_ptr->queue_start_time);
    }
    AddPendingCheck(check_ptr);

    GetPrefixMatches(
        check_ptr,
        base::BindOnce(&SBLocalDatabaseManager::ProcessQueuedChecksContinuation,
                       weak_factory_.GetWeakPtr(), std::move(it)));
  }
}

void SBLocalDatabaseManager::ProcessQueuedChecksContinuation(
    std::unique_ptr<PendingCheck> check,
    DbLookupResult lookup_result) {
  if (!IsDatabaseReady()) {
    DCHECK(pending_checks_.empty());
    return;
  }

  base::UmaHistogramTimes(
      "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup",
      base::TimeTicks::Now() - check->local_db_lookup_start_time);

  base::UmaHistogramTimes(
      "SafeBrowsing.V4CheckUrl.TimeTaken.LocalLookup.UiCallbackQueueDelay",
      base::TimeTicks::Now() - lookup_result.db_thread_end_time);

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  RemovePendingCheck(it);

  check->full_hash_to_store_and_hash_prefixes = lookup_result.results;
  GetArtificialPrefixMatches(check);
  RespondOrScheduleFullHashCheck(std::move(check));
}

void SBLocalDatabaseManager::RespondSafeToQueuedAndPendingChecks() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);
  for (std::unique_ptr<PendingCheck>& it : checks) {
    RespondToClient(std::move(it));
  }

  // Clear pending_checks_ up front and iterate through a copy to avoid the
  // possibility of concurrent modifications while iterating.
  PendingChecks pending_checks = CopyAndRemoveAllPendingChecks();
  for (PendingCheck* it : pending_checks) {
    if (it->client_callback_type == ClientCallbackType::CHECK_OTHER) {
      // In this case there's a callback that will run when weak_factory_ is
      // invalidated.
      continue;
    }
    // We don't own the unique pointer for the pending check, so we do not
    // perform cleanup on it while responding to the client.
    RespondToClientWithoutPendingCheckCleanup(it);
  }
}

void SBLocalDatabaseManager::DropQueuedAndPendingChecks() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  queued_checks_.clear();
  // Abandon all checks this method returns to avoid dangling raw pointers.
  PendingChecks pending_checks = CopyAndRemoveAllPendingChecks();
  for (PendingCheck* check : pending_checks) {
    check->Abandon();
  }
}

void SBLocalDatabaseManager::RespondToClient(
    std::unique_ptr<PendingCheck> check) {
  RespondToClientWithoutPendingCheckCleanup(check.get());
}

void SBLocalDatabaseManager::RespondOrScheduleFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  CHECK(check->needs_full_hash_check_after_local_match.has_value());
  if ((check->full_hash_to_store_and_hash_prefixes.empty() &&
       check->artificial_full_hash_to_store_and_hash_prefixes.empty()) ||
      !check->needs_full_hash_check_after_local_match.value()) {
    RespondToClient(std::move(check));
  } else {
    check->get_full_hash_queue_start_time = base::TimeTicks::Now();
    ScheduleFullHashCheck(std::move(check));
  }
}

void SBLocalDatabaseManager::RespondToClientWithoutPendingCheckCleanup(
    PendingCheck* check) {
  CHECK(check);
  CHECK(check->client ||
        check->client_callback_type == ClientCallbackType::CHECK_OTHER);

  // Responding to the client may cause deletion of the client. Reset the member
  // so it's not dangling.
  Client* client = check->client;
  check->client = nullptr;

  switch (check->client_callback_type) {
    case ClientCallbackType::CHECK_BROWSE_URL:
      DCHECK_EQ(1u, check->urls.size());
      client->OnCheckBrowseUrlResult(check->urls[0],
                                     check->most_severe_threat_type);
      break;

    case ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER:
      DCHECK_EQ(1u, check->urls.size());
      client->OnCheckSubresourceFilterUrlResult(
          check->urls[0], check->most_severe_threat_type,
          check->url_metadata.subresource_filter_match);
      break;

    case ClientCallbackType::CHECK_DOWNLOAD_URLS:
      client->OnCheckDownloadUrlResult(check->urls,
                                       check->most_severe_threat_type);
      break;

    case ClientCallbackType::CHECK_CSD_ALLOWLIST: {
      DCHECK_EQ(1u, check->urls.size());
      bool did_match_allowlist = check->most_severe_threat_type ==
                                 SBThreatType::SB_THREAT_TYPE_CSD_ALLOWLIST;
      DCHECK(did_match_allowlist || check->most_severe_threat_type ==
                                        SBThreatType::SB_THREAT_TYPE_SAFE);
      client->OnCheckAllowlistUrlResult(did_match_allowlist);
      break;
    }

    case ClientCallbackType::CHECK_EXTENSION_IDS: {
      std::set<FullHashStr> unsafe_extension_ids;
      CHECK(check->needs_full_hash_check_after_local_match.has_value());
      if (!check->needs_full_hash_check_after_local_match.value()) {
        // Populate unsafe_extension_ids directly from local database match
        // keys.
        for (const auto& entry : check->full_hash_to_store_and_hash_prefixes) {
          unsafe_extension_ids.insert(entry.first);
        }
      } else {
        DCHECK_EQ(check->full_hash_threat_types.size(),
                  check->full_hashes.size());
        for (size_t i = 0; i < check->full_hash_threat_types.size(); i++) {
          if (check->full_hash_threat_types[i] ==
              SBThreatType::SB_THREAT_TYPE_EXTENSION) {
            unsafe_extension_ids.insert(check->full_hashes[i]);
          }
        }
      }
      client->OnCheckExtensionsResult(unsafe_extension_ids);
      break;
    }

    case ClientCallbackType::CHECK_OTHER:
      NOTREACHED() << "Unexpected client_callback_type encountered";
  }
}

void SBLocalDatabaseManager::SetupDatabase() {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Do not create the database on the UI thread since this may be an expensive
  // operation. Instead, do that on the task_runner and when the new database
  // has been created, swap it out on the UI thread.
  NewDatabaseReadyCallback db_ready_callback =
      base::BindOnce(&SBLocalDatabaseManager::DatabaseReadyForChecks,
                     weak_factory_.GetWeakPtr(), base::Time::Now());
  SBDatabase::Create(task_runner_, base_path_, list_infos_,
                     std::move(db_ready_callback));
}

void SBLocalDatabaseManager::SetupUpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    V5UpdateCallback update_callback =
        base::BindRepeating(&SBLocalDatabaseManager::V5UpdateRequestCompleted,
                            weak_factory_.GetWeakPtr());
    update_protocol_manager_ = std::make_unique<V5UpdateProtocolManager>(
        url_loader_factory, config, update_callback);
  } else {
    V4UpdateCallback update_callback =
        base::BindRepeating(&SBLocalDatabaseManager::V4UpdateRequestCompleted,
                            weak_factory_.GetWeakPtr());
    update_protocol_manager_ = std::make_unique<V4UpdateProtocolManager>(
        url_loader_factory, config, update_callback,
        extended_reporting_level_callback_);
  }
}

void SBLocalDatabaseManager::V4UpdateRequestCompleted(
    std::unique_ptr<ParsedServerResponse> parsed_server_response) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  auto update_map = std::make_unique<SBUpdateResponseMap>();
  for (auto& response : *parsed_server_response) {
    ListIdentifier identifier(*response);
    auto sb_response = std::make_unique<SBUpdateResponse>();
    sb_response->v4_response = std::move(response);
    update_map->insert({identifier, std::move(sb_response)});
  }
  sb_database_->ApplyUpdate(std::move(update_map), db_updated_callback_);
}

void SBLocalDatabaseManager::V5UpdateRequestCompleted(
    std::optional<std::map<ListIdentifier, V5::HashList>>
        parsed_server_response) {
  CHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  // TODO(crbug.com/362791941): Remove nullopt case from API since it never
  // occurs.
  if (!parsed_server_response.has_value()) {
    return;
  }

  auto update_map = std::make_unique<SBUpdateResponseMap>();
  for (auto& [identifier, response] : parsed_server_response.value()) {
    auto sb_response = std::make_unique<SBUpdateResponse>();
    sb_response->v5_response =
        std::make_unique<V5::HashList>(std::move(response));
    update_map->insert({identifier, std::move(sb_response)});
  }
  sb_database_->ApplyUpdate(std::move(update_map), db_updated_callback_);
}

bool SBLocalDatabaseManager::AreAllStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return IsDatabaseReady() &&
         sb_database_->AreAllStoresAvailable(stores_to_check);
}

int64_t SBLocalDatabaseManager::GetStoreEntryCount(const ListIdentifier& store,
                                                   int bytes_per_entry) const {
  if (!IsDatabaseReady()) {
    return 0;
  }
  return sb_database_->GetStoreSizeInBytes(store) / bytes_per_entry;
}

bool SBLocalDatabaseManager::IsStoreTooSmall(const ListIdentifier& store,
                                             int bytes_per_entry,
                                             int min_entry_count) const {
  return GetStoreEntryCount(store, bytes_per_entry) < min_entry_count;
}

bool SBLocalDatabaseManager::AreAnyStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return IsDatabaseReady() &&
         sb_database_->AreAnyStoresAvailable(stores_to_check);
}

void SBLocalDatabaseManager::UpdateListClientStates(
    const std::unique_ptr<StoreStateMap>& store_state_map) {
  list_client_states_.clear();
  SBProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
      store_state_map, &list_client_states_);
}

void SBLocalDatabaseManager::AddPendingCheck(PendingCheck* check) {
  check->is_in_pending_checks = true;
  pending_checks_.insert(check);
}

void SBLocalDatabaseManager::RemovePendingCheck(
    PendingChecks::const_iterator it) {
  (*it)->is_in_pending_checks = false;
  pending_checks_.erase(it);
}

SBLocalDatabaseManager::PendingChecks
SBLocalDatabaseManager::CopyAndRemoveAllPendingChecks() {
  PendingChecks pending_checks;
  pending_checks.swap(pending_checks_);
  for (PendingCheck* check : pending_checks) {
    check->is_in_pending_checks = false;
  }
  return pending_checks;
}

}  // namespace safe_browsing
