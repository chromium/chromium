// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

// If the switch is present, any high-confidence allowlist check will return
// that it does not match the allowlist.
const char kSkipHighConfidenceAllowlist[] =
    "safe-browsing-skip-high-confidence-allowlist";

const ThreatSeverity kLeastSeverity =
    std::numeric_limits<ThreatSeverity>::max();

const char* const kStoreFileNamesToDelete[] = {"IpMalware.store"};

ListInfos GetListInfos() {
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
      ListInfo(kSyncAlways, "UrlSoceng.store", GetUrlSocEngId(),
               SB_THREAT_TYPE_URL_PHISHING),
      ListInfo(kSyncAlways, "UrlMalware.store", GetUrlMalwareId(),
               SB_THREAT_TYPE_URL_MALWARE),
      ListInfo(kSyncAlways, "UrlUws.store", GetUrlUwsId(),
               SB_THREAT_TYPE_URL_UNWANTED),
      ListInfo(kSyncOnDesktopBuilds, "UrlMalBin.store", GetUrlMalBinId(),
               SB_THREAT_TYPE_URL_BINARY_MALWARE),
      ListInfo(kSyncOnDesktopBuilds, "ChromeExtMalware.store",
               GetChromeExtMalwareId(), SB_THREAT_TYPE_EXTENSION),
      ListInfo(kSyncOnChromeDesktopBuilds, "ChromeUrlClientIncident.store",
               GetChromeUrlClientIncidentId(),
               SB_THREAT_TYPE_BLOCKLISTED_RESOURCE),
      ListInfo(kSyncAlways, "UrlBilling.store", GetUrlBillingId(),
               SB_THREAT_TYPE_BILLING),
      ListInfo(kSyncOnDesktopBuilds, "UrlCsdDownloadAllowlist.store",
               GetUrlCsdDownloadAllowlistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnChromeDesktopBuilds || kSyncOnIos,
               "UrlCsdAllowlist.store", GetUrlCsdAllowlistId(),
               SB_THREAT_TYPE_CSD_ALLOWLIST),
      ListInfo(kSyncOnChromeDesktopBuilds, "UrlSubresourceFilter.store",
               GetUrlSubresourceFilterId(), SB_THREAT_TYPE_SUBRESOURCE_FILTER),
      ListInfo(kSyncOnChromeDesktopBuilds, "UrlSuspiciousSite.store",
               GetUrlSuspiciousSiteId(), SB_THREAT_TYPE_SUSPICIOUS_SITE),
      ListInfo(kSyncNever, "", GetChromeUrlApiId(), SB_THREAT_TYPE_API_ABUSE),
      ListInfo(kSyncOnChromeDesktopBuilds || kSyncOnIos,
               "UrlHighConfidenceAllowlist.store",
               GetUrlHighConfidenceAllowlistId(),
               SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST),
  });
  // NOTE(vakh): IMPORTANT: Please make sure that the server already supports
  // any list before adding it to this list otherwise the prefix updates break
  // for all Canary users.
}

base::span<const CommandLineSwitchAndThreatType> GetSwitchAndThreatTypes() {
  static constexpr CommandLineSwitchAndThreatType
      kCommandLineSwitchAndThreatType[] = {
          {"mark_as_allowlisted_for_phish_guard", CSD_WHITELIST},
          {"mark_as_allowlisted_for_real_time", HIGH_CONFIDENCE_ALLOWLIST},
          {"mark_as_phishing", SOCIAL_ENGINEERING},
          {"mark_as_malware", MALWARE_THREAT},
          {"mark_as_uws", UNWANTED_SOFTWARE}};
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
    case CLIENT_INCIDENT:
    case SUBRESOURCE_FILTER:
      return 2;
    case CSD_WHITELIST:
    case HIGH_CONFIDENCE_ALLOWLIST:
      return 3;
    case SUSPICIOUS:
      return 4;
    case BILLING:
      return 15;
    case CSD_DOWNLOAD_WHITELIST:
    case POTENTIALLY_HARMFUL_APPLICATION:
    case SOCIAL_ENGINEERING_PUBLIC:
    case THREAT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected ThreatType encountered: "
                   << list_id.threat_type();
      return kLeastSeverity;
  }
}

// This is only valid for types that are passed to GetBrowseUrl().
ListIdentifier GetUrlIdFromSBThreatType(SBThreatType sb_threat_type) {
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

    default:
      NOTREACHED();
      // Compiler requires a return statement here.
      return GetUrlMalwareId();
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

void RecordCheckUrlForHighConfidenceAllowlistBoolean(
    const std::string& metric_name,
    const std::string& metric_variation,
    bool value) {
  auto histogram_name =
      base::StrCat({"SafeBrowsing.", metric_variation, ".", metric_name});
  DCHECK(histogram_name == "SafeBrowsing.RT.AllStoresAvailable" ||
         histogram_name == "SafeBrowsing.HPRT.AllStoresAvailable" ||
         histogram_name == "SafeBrowsing.RT.AllowlistSizeTooSmall" ||
         histogram_name == "SafeBrowsing.HPRT.AllowlistSizeTooSmall");
  base::UmaHistogramBoolean(histogram_name, value);
}

void MaybeDeleteStore(const base::FilePath& path) {
  bool path_exists = base::PathExists(path);
  base::UmaHistogramBoolean(
      "SafeBrowsing.V4UnusedStoreFileExists" + GetUmaSuffixForStore(path),
      path_exists);

  // The MmapHashPrefixMap maintains several helper files stored in the same
  // directory as the main store file. These are usually found by looking at the
  // `hash_files` field in the `V4StoreFileFormat`, but we haven't read the
  // store at this point. Instead we use the fact that these helper files have a
  // simple structure to delete them all.
  std::vector<base::FilePath> paths_to_delete;
  base::FileEnumerator enumerator(
      path.DirName(), false, base::FileEnumerator::FILES,
      path.BaseName().value() + FILE_PATH_LITERAL("*"),
      // Since the search is non-recursive and only on files, the folder search
      // policy doesn't matter. We set it to the default value here.
      base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (base::FilePath store_path = enumerator.Next(); !store_path.empty();
       store_path = enumerator.Next()) {
    paths_to_delete.push_back(std::move(store_path));
  }

  for (const base::FilePath& delete_path : paths_to_delete) {
    base::DeleteFile(delete_path);
  }
}

bool GetPrefixMatchesIsAsync() {
  return base::FeatureList::IsEnabled(kMmapSafeBrowsingDatabase) &&
         kMmapSafeBrowsingDatabaseAsync.Get();
}

void HandleUrlCallback(base::OnceCallback<void(bool)> callback,
                       FullHashToStoreAndHashPrefixesMap results) {
  bool allowed = !results.empty();
  if (GetPrefixMatchesIsAsync()) {
    // This callback was already run asynchronously so no need for another
    // thread hop.
    std::move(callback).Run(allowed);
  } else {
    // Need a thread hop to avoid reentrancy.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), allowed));
  }
}

}  // namespace

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::vector<GURL>& urls,
    MechanismExperimentHashDatabaseCache experiment_cache_selection)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check),
      urls(urls),
      mechanism_experiment_cache_selection(experiment_cache_selection) {
  for (const auto& url : urls) {
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  }
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);
}

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::set<FullHashStr>& full_hashes_set)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check) {
  full_hashes.assign(full_hashes_set.begin(), full_hashes_set.end());
  DCHECK(full_hashes.size());
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);
}

V4LocalDatabaseManager::PendingCheck::~PendingCheck() {
  DCHECK(!is_in_pending_checks);
}

// static
const V4LocalDatabaseManager*
    V4LocalDatabaseManager::current_local_database_manager_;

// static
scoped_refptr<V4LocalDatabaseManager> V4LocalDatabaseManager::Create(
    const base::FilePath& base_path,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    ExtendedReportingLevelCallback extended_reporting_level_callback,
    RecordMigrationMetricsCallback record_migration_metrics_callback) {
  return base::WrapRefCounted(new V4LocalDatabaseManager(
      base_path, extended_reporting_level_callback,
      std::move(record_migration_metrics_callback), std::move(ui_task_runner),
      std::move(io_task_runner), nullptr));
}

void V4LocalDatabaseManager::CollectDatabaseManagerInfo(
    DatabaseManagerInfo* database_manager_info,
    FullHashCacheInfo* full_hash_cache_info) const {
  if (v4_update_protocol_manager_) {
    v4_update_protocol_manager_->CollectUpdateInfo(
        database_manager_info->mutable_update_info());
  }
  if (v4_database_) {
    v4_database_->CollectDatabaseInfo(
        database_manager_info->mutable_database_info());
  }
  if (v4_get_hash_protocol_manager_) {
    v4_get_hash_protocol_manager_->CollectFullHashCacheInfo(
        full_hash_cache_info);
  }
}

V4LocalDatabaseManager::V4LocalDatabaseManager(
    const base::FilePath& base_path,
    ExtendedReportingLevelCallback extended_reporting_level_callback,
    RecordMigrationMetricsCallback record_migration_metrics_callback,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_tests)
    : SafeBrowsingDatabaseManager(std::move(ui_task_runner),
                                  std::move(io_task_runner)),
      base_path_(base_path),
      extended_reporting_level_callback_(extended_reporting_level_callback),
      record_migration_metrics_callback_(
          std::move(record_migration_metrics_callback)),
      list_infos_(GetListInfos()),
      task_runner_(task_runner_for_tests
                       ? task_runner_for_tests
                       : base::ThreadPool::CreateSequencedTaskRunner(
                             {base::MayBlock(),
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      v4_database_(std::unique_ptr<V4Database, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(nullptr))),
      enabled_(false),
      is_shutdown_(false) {
  DCHECK(this->ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());

  DeleteUnusedStoreFiles();
}

V4LocalDatabaseManager::~V4LocalDatabaseManager() {
  DCHECK(!enabled_);
}

//
// Start: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::CancelCheck(Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  // If we've stopped responding due to browser shutdown, it's possible that a
  // client will call CancelCheck even though we're disabled. Note that we can't
  // use IsDatabaseReady() here because there's several expected cases where a
  // client could cancel while the request is still queued (e.g. timeouts, tab
  // being closed).
  DCHECK(enabled_ || is_shutdown_);
  auto pending_it =
      base::ranges::find(pending_checks_, client, &PendingCheck::client);
  if (pending_it != pending_checks_.end()) {
    RemovePendingCheck(pending_it);
  }

  auto queued_it =
      base::ranges::find(queued_checks_, client, &PendingCheck::client);
  if (queued_it != queued_checks_.end()) {
    queued_checks_.erase(queued_it);
  }
}

bool V4LocalDatabaseManager::CanCheckRequestDestination(
    network::mojom::RequestDestination request_destination) const {
  // We check all destinations since most checks are fast.
  return true;
}

bool V4LocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}

bool V4LocalDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool V4LocalDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    MechanismExperimentHashDatabaseCache experiment_cache_selection,
    CheckBrowseUrlType check_type) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!threat_types.empty());
  DCHECK(SBThreatTypeSetIsValidForCheckBrowseUrl(threat_types));
  DCHECK(check_type == CheckBrowseUrlType::kHashDatabase)
      << "V4 Local database only support hash database check.";

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_BROWSE_URL,
      CreateStoresToCheckFromSBThreatTypeSet(threat_types),
      std::vector<GURL>(1, url), experiment_cache_selection);

  bool safe_synchronously = HandleCheck(std::move(check));
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.CheckBrowseUrl.HasLocalMatch",
                        !safe_synchronously);
  RecordTimeSinceLastUpdateHistograms(
      v4_update_protocol_manager_->last_response_time());
  return safe_synchronously;
}

bool V4LocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_ || url_chain.empty()) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_DOWNLOAD_URLS,
      StoresToCheck({GetUrlMalBinId()}), url_chain,
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckExtensionIDs(
    const std::set<FullHashStr>& extension_ids,
    Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  // We use `enabled_` here because `HandleCheck` queues checks that come in
  // before the database is ready.
  if (!enabled_) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_EXTENSION_IDS,
      StoresToCheck({GetChromeExtMalwareId()}), extension_ids);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckResourceUrl(const GURL& url, Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check({GetChromeUrlClientIncidentId()});

  if (!CanCheckUrl(url) || !AreAllStoresAvailableNow(stores_to_check)) {
    // Fail open: Mark resource as safe immediately.
    // TODO(nparker): This should queue the request if the DB isn't yet
    // loaded, and later decide if this store is available.
    // Currently this is the only store that requires full-hash-checks
    // AND isn't supported on Chromium, so it's unique.
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_RESOURCE_URL, stores_to_check,
      std::vector<GURL>(1, url),
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  return HandleCheck(std::move(check));
}

void V4LocalDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    const std::string& metric_variation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSkipHighConfidenceAllowlist)) {
    sb_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
    return;
  }

  StoresToCheck stores_to_check({GetUrlHighConfidenceAllowlistId()});
  bool all_stores_available = AreAllStoresAvailableNow(stores_to_check);
  RecordCheckUrlForHighConfidenceAllowlistBoolean(
      "AllStoresAvailable", metric_variation, all_stores_available);
  bool is_artificial_prefix_empty =
      artificially_marked_store_and_hash_prefixes_.empty();
  bool is_allowlist_too_small =
      IsStoreTooSmall(GetUrlHighConfidenceAllowlistId(), kBytesPerFullHashEntry,
                      kHighConfidenceAllowlistMinimumEntryCount);
  RecordCheckUrlForHighConfidenceAllowlistBoolean(
      "AllowlistSizeTooSmall", metric_variation, is_allowlist_too_small);
  if (!IsDatabaseReady() ||
      (is_allowlist_too_small && is_artificial_prefix_empty) ||
      !CanCheckUrl(url) ||
      (!all_stores_available && is_artificial_prefix_empty)) {
    // NOTE(vakh): If Safe Browsing isn't enabled yet, or if the URL isn't a
    // navigation URL, or if the allowlist isn't ready yet, or if the allowlist
    // is too small, return that there is a match. The full URL check won't be
    // performed, but hash-based check will still be done. If any artificial
    // matches are present, consider the allowlist as ready.
    sb_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), true));
    return;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url),
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  HandleAllowlistCheck(std::move(check), /*allow_async_full_hash_check=*/false,
                       std::move(callback));
}

bool V4LocalDatabaseManager::CheckUrlForSubresourceFilter(const GURL& url,
                                                          Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check(
      {GetUrlSocEngId(), GetUrlSubresourceFilterId()});
  if (!AreAnyStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER,
      stores_to_check, std::vector<GURL>(1, url),
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  return HandleCheck(std::move(check));
}

AsyncMatch V4LocalDatabaseManager::CheckCsdAllowlistUrl(const GURL& url,
                                                        Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

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
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  return HandleAllowlistCheck(std::move(check),
                              /*allow_async_full_hash_check=*/true,
                              base::OnceCallback<void(bool)>());
}

void V4LocalDatabaseManager::MatchDownloadAllowlistUrl(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  StoresToCheck stores_to_check({GetUrlCsdDownloadAllowlistId()});

  if (!AreAllStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    // Fail close: Allowlist nothing. This may generate download-protection
    // pings for allowlisted domains, but that's fine.
    sb_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
    return;
  }

  HandleUrl(url, stores_to_check, std::move(callback));
}

ThreatSource V4LocalDatabaseManager::GetBrowseUrlThreatSource(
    CheckBrowseUrlType check_type) const {
  DCHECK(check_type == CheckBrowseUrlType::kHashDatabase)
      << "V4 Local database only support hash database check.";
  return ThreatSource::LOCAL_PVER4;
}

ThreatSource V4LocalDatabaseManager::GetNonBrowseUrlThreatSource() const {
  return ThreatSource::LOCAL_PVER4;
}

bool V4LocalDatabaseManager::IsDownloadProtectionEnabled() const {
  return true;
}

void V4LocalDatabaseManager::StartOnSBThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnSBThread(url_loader_factory, config);

  db_updated_callback_ = base::BindRepeating(
      &V4LocalDatabaseManager::DatabaseUpdated, weak_factory_.GetWeakPtr());

  SetupUpdateProtocolManager(url_loader_factory, config);
  SetupDatabase();

  enabled_ = true;
  is_shutdown_ = false;

  current_local_database_manager_ = this;
}

void V4LocalDatabaseManager::StopOnSBThread(bool shutdown) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  enabled_ = false;
  is_shutdown_ = shutdown;

  current_local_database_manager_ = nullptr;

  // On shutdown, it's acceptable to fail to respond.
  if (shutdown) {
    DropQueuedAndPendingChecks();
  } else {
    RespondSafeToQueuedAndPendingChecks();
  }

  // Delete the V4Database. Any pending writes to disk are completed.
  // This operation happens on the task_runner on which v4_database_ operates
  // and doesn't block the IO thread.
  if (v4_database_) {
    v4_database_->StopOnSBThread();
  }
  v4_database_.reset();

  // Delete the V4UpdateProtocolManager.
  // This cancels any in-flight update request.
  v4_update_protocol_manager_.reset();

  db_updated_callback_.Reset();

  weak_factory_.InvalidateWeakPtrs();

  SafeBrowsingDatabaseManager::StopOnSBThread(shutdown);
}

bool V4LocalDatabaseManager::IsDatabaseReady() const {
  return enabled_ && !!v4_database_;
}

//
// End: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::DatabaseReadyForChecks(
    std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  v4_database->InitializeOnSBThread();

  // The following check is needed because it is possible that by the time the
  // database is ready, StopOnSBThread has been called.
  if (enabled_) {
    v4_database_ = std::move(v4_database);

    v4_database_->RecordFileSizeHistograms();
    if (record_migration_metrics_callback_) {
      ui_task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(record_migration_metrics_callback_),
                         v4_database_->GetMigrateResult()));
    }

    PopulateArtificialDatabase();

    // The consistency of the stores read from the disk needs to verified. Post
    // that task on the task runner. It calls |DatabaseReadyForUpdates|
    // callback with the stores to reset, if any, and then we can schedule the
    // database updates.
    v4_database_->VerifyChecksum(
        base::BindOnce(&V4LocalDatabaseManager::DatabaseReadyForUpdates,
                       weak_factory_.GetWeakPtr()));

    ProcessQueuedChecks();
  } else {
    // Schedule the deletion of v4_database off IO thread.
    v4_database.reset();
  }
}

void V4LocalDatabaseManager::DatabaseReadyForUpdates(
    const std::vector<ListIdentifier>& stores_to_reset) {
  if (IsDatabaseReady()) {
    v4_database_->ResetStores(stores_to_reset);
    UpdateListClientStates(GetStoreStateMap());

    // The database is ready to process updates. Schedule them now.
    v4_update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());
  }
}

void V4LocalDatabaseManager::DatabaseUpdated() {
  if (IsDatabaseReady()) {
    v4_update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());

    v4_database_->RecordFileSizeHistograms();
    UpdateListClientStates(GetStoreStateMap());

    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished, this));
  }
}

void V4LocalDatabaseManager::GetArtificialPrefixMatches(
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
          base::Contains(check->stores_to_check,
                         artificial_store_and_hash_prefix.list_id)) {
        (check->artificial_full_hash_to_store_and_hash_prefixes)[full_hash] = {
            artificial_store_and_hash_prefix};
      }
    }
  }
}

void V4LocalDatabaseManager::GetPrefixMatches(
    PendingCheck* check,
    base::OnceCallback<void(FullHashToStoreAndHashPrefixesMap)> callback) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(IsDatabaseReady());

  v4_database_->GetStoresMatchingFullHash(
      check->full_hashes, check->stores_to_check, std::move(callback));
}

void V4LocalDatabaseManager::GetSeverestThreatTypeAndMetadata(
    const std::vector<FullHashInfo>& full_hash_infos,
    const std::vector<FullHashStr>& full_hashes,
    std::vector<SBThreatType>* full_hash_threat_types,
    SBThreatType* most_severe_threat_type,
    ThreatMetadata* metadata,
    FullHashStr* matching_full_hash) {
  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.V4LocalDatabaseManager.ThreatInfoSize",
                           full_hash_infos.size());
  ThreatSeverity most_severe_yet = kLeastSeverity;
  for (const FullHashInfo& fhi : full_hash_infos) {
    ThreatSeverity severity = GetThreatSeverity(fhi.list_id);
    SBThreatType threat_type = GetSBThreatTypeForList(fhi.list_id);

    const auto& it = base::ranges::find(full_hashes, fhi.full_hash);
    DCHECK(it != full_hashes.end());
    (*full_hash_threat_types)[it - full_hashes.begin()] = threat_type;

    if (severity < most_severe_yet) {
      most_severe_yet = severity;
      *most_severe_threat_type = threat_type;
      *metadata = fhi.metadata;
      *matching_full_hash = fhi.full_hash;
    }
  }
}

StoresToCheck V4LocalDatabaseManager::GetStoresForFullHashRequests() {
  StoresToCheck stores_for_full_hash;
  for (const auto& info : list_infos_) {
    stores_for_full_hash.insert(info.list_id());
  }
  return stores_for_full_hash;
}

std::unique_ptr<StoreStateMap> V4LocalDatabaseManager::GetStoreStateMap() {
  return v4_database_->GetStoreStateMap();
}

// Returns the SBThreatType corresponding to a given SafeBrowsing list.
SBThreatType V4LocalDatabaseManager::GetSBThreatTypeForList(
    const ListIdentifier& list_id) {
  auto it = base::ranges::find(list_infos_, list_id, &ListInfo::list_id);
  DCHECK(list_infos_.end() != it);
  DCHECK_NE(SB_THREAT_TYPE_SAFE, it->sb_threat_type());
  DCHECK_NE(SB_THREAT_TYPE_UNUSED, it->sb_threat_type());
  return it->sb_threat_type();
}

AsyncMatch V4LocalDatabaseManager::HandleAllowlistCheck(
    std::unique_ptr<PendingCheck> check,
    bool allow_async_full_hash_check,
    base::OnceCallback<void(bool)> callback) {
  // We don't bother queuing allowlist checks since the DB will
  // normally be available already -- allowlists are used after page load,
  // and navigations are blocked until the DB is ready and dequeues checks.
  // The caller should have already checked that the DB is ready.
  DCHECK(v4_database_);

  PendingCheck* check_ptr = check.get();
  AsyncMatch match;

  if (GetPrefixMatchesIsAsync() && !callback.is_null()) {
    // If StopOnSBThread is called weak_factory_ will get invalidated and
    // HandleAllowlistCheckContinuation won't be called. We still want to run
    // the callback though. See comment in CheckUrlForHighConfidenceAllowlist
    // on why this returns true.
    callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), true);
  }

  GetPrefixMatches(
      check_ptr,
      base::BindOnce(&V4LocalDatabaseManager::HandleAllowlistCheckContinuation,
                     weak_factory_.GetWeakPtr(), std::move(check),
                     allow_async_full_hash_check, std::move(callback),
                     GetPrefixMatchesIsAsync() ? nullptr : &match));

  if (GetPrefixMatchesIsAsync()) {
    AddPendingCheck(check_ptr);
    return AsyncMatch::ASYNC;
  }

  return match;
}

void V4LocalDatabaseManager::HandleAllowlistCheckContinuation(
    std::unique_ptr<PendingCheck> check,
    bool allow_async_full_hash_check,
    base::OnceCallback<void(bool)> callback,
    AsyncMatch* match,
    FullHashToStoreAndHashPrefixesMap results) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  AsyncMatch local_match;
  if (GetPrefixMatchesIsAsync()) {
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
    match = &local_match;
  }

  check->full_hash_to_store_and_hash_prefixes = results;
  GetArtificialPrefixMatches(check);
  if (check->full_hash_to_store_and_hash_prefixes.empty() &&
      check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    *match = AsyncMatch::NO_MATCH;
  } else {
    // Look for any full-length hash in the matches. If there is one,
    // there's no need for a full-hash check. This saves bandwidth for
    // very popular sites since they'll have full-length hashes locally.
    // These loops will have exactly 1 entry most of the time.
    bool found = false;
    for (const auto& entry : check->full_hash_to_store_and_hash_prefixes) {
      for (const auto& store_and_prefix : entry.second) {
        if (store_and_prefix.hash_prefix.size() == kMaxHashPrefixLength) {
          *match = AsyncMatch::MATCH;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      if (!allow_async_full_hash_check) {
        *match = AsyncMatch::NO_MATCH;
      } else {
        *match = AsyncMatch::ASYNC;
        ScheduleFullHashCheck(std::move(check));
        return;
      }
    }
  }

  if (check->client_callback_type == ClientCallbackType::CHECK_OTHER) {
    bool result = *match == AsyncMatch::MATCH;
    if (GetPrefixMatchesIsAsync()) {
      // This is already asynchronous so no need for another PostTask.
      std::move(callback).Run(result);
    } else {
      sb_task_runner()->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), result));
    }
  } else if (check->client_callback_type ==
             ClientCallbackType::CHECK_CSD_ALLOWLIST) {
    if (GetPrefixMatchesIsAsync()) {
      check->most_severe_threat_type = SB_THREAT_TYPE_CSD_ALLOWLIST;
      RespondToClient(std::move(check));
    }
  } else {
    NOTREACHED();
  }
}

bool V4LocalDatabaseManager::HandleCheck(std::unique_ptr<PendingCheck> check) {
  if (!v4_database_) {
    queued_checks_.push_back(std::move(check));
    return false;
  }

  PendingCheck* check_ptr = check.get();
  AsyncMatch match;
  GetPrefixMatches(
      check_ptr,
      base::BindOnce(&V4LocalDatabaseManager::HandleCheckContinuation,
                     weak_factory_.GetWeakPtr(), std::move(check),
                     GetPrefixMatchesIsAsync() ? nullptr : &match));

  if (GetPrefixMatchesIsAsync()) {
    AddPendingCheck(check_ptr);
    return false;
  }

  return match == AsyncMatch::NO_MATCH;
}

void V4LocalDatabaseManager::HandleCheckContinuation(
    std::unique_ptr<PendingCheck> check,
    AsyncMatch* match,
    FullHashToStoreAndHashPrefixesMap results) {
  AsyncMatch local_match;
  if (GetPrefixMatchesIsAsync()) {
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
    match = &local_match;
  }

  check->full_hash_to_store_and_hash_prefixes = results;
  GetArtificialPrefixMatches(check);
  if (check->full_hash_to_store_and_hash_prefixes.empty() &&
      check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    *match = AsyncMatch::NO_MATCH;

    if (GetPrefixMatchesIsAsync()) {
      RespondToClient(std::move(check));
    }
  } else {
    *match = AsyncMatch::ASYNC;

    ScheduleFullHashCheck(std::move(check));
  }
}

void V4LocalDatabaseManager::PopulateArtificialDatabase() {
  for (const auto& switch_and_threat_type : GetSwitchAndThreatTypes()) {
    const std::string raw_artificial_urls =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switch_and_threat_type.cmdline_switch);
    base::StringTokenizer tokenizer(raw_artificial_urls, ",");
    while (tokenizer.GetNext()) {
      ListIdentifier artificial_list_id(GetCurrentPlatformType(), URL,
                                        switch_and_threat_type.threat_type);
      FullHashStr full_hash =
          V4ProtocolManagerUtil::GetFullHash(GURL(tokenizer.token_piece()));
      artificially_marked_store_and_hash_prefixes_.emplace_back(
          artificial_list_id, full_hash);
    }
  }
}

void V4LocalDatabaseManager::ScheduleFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

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

    sb_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4LocalDatabaseManager::OnFullHashResponse,
                                  weak_factory_.GetWeakPtr(), std::move(check),
                                  full_hash_infos));
  } else {
    // Post on the SB thread to enforce async behavior.
    sb_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&V4LocalDatabaseManager::PerformFullHashCheck,
                       weak_factory_.GetWeakPtr(), std::move(check)));
  }
}

void V4LocalDatabaseManager::HandleUrl(
    const GURL& url,
    const StoresToCheck& stores_to_check,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url),
      MechanismExperimentHashDatabaseCache::kNoExperiment);

  GetPrefixMatches(check.get(),
                   base::BindOnce(&HandleUrlCallback, std::move(callback)));
}

void V4LocalDatabaseManager::OnFullHashResponse(
    std::unique_ptr<PendingCheck> check,
    const std::vector<FullHashInfo>& full_hash_infos) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  if (!IsDatabaseReady()) {
    DCHECK(pending_checks_.empty());
    return;
  }

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  // Find out the most severe threat, if any, to report to the client.
  GetSeverestThreatTypeAndMetadata(
      full_hash_infos, check->full_hashes, &check->full_hash_threat_types,
      &check->most_severe_threat_type, &check->url_metadata,
      &check->matching_full_hash);
  RemovePendingCheck(it);
  RespondToClient(std::move(check));
}

void V4LocalDatabaseManager::PerformFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  DCHECK(!check->full_hash_to_store_and_hash_prefixes.empty());

  // If the database isn't ready, the service has been turned off, so silently
  // drop the check.
  if (IsDatabaseReady()) {
    FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes =
        check->full_hash_to_store_and_hash_prefixes;
    MechanismExperimentHashDatabaseCache experiment_cache_selection =
        check->mechanism_experiment_cache_selection;
    v4_get_hash_protocol_manager_->GetFullHashes(
        full_hash_to_store_and_hash_prefixes, list_client_states_,
        base::BindOnce(&V4LocalDatabaseManager::OnFullHashResponse,
                       weak_factory_.GetWeakPtr(), std::move(check)),
        experiment_cache_selection);
  } else {
    DCHECK(pending_checks_.empty());
  }
}

void V4LocalDatabaseManager::ProcessQueuedChecks() {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (auto& it : checks) {
    PendingCheck* check_ptr = it.get();

    if (GetPrefixMatchesIsAsync()) {
      AddPendingCheck(check_ptr);
    }

    GetPrefixMatches(
        check_ptr,
        base::BindOnce(&V4LocalDatabaseManager::ProcessQueuedChecksContinuation,
                       weak_factory_.GetWeakPtr(), std::move(it)));
  }
}

void V4LocalDatabaseManager::ProcessQueuedChecksContinuation(
    std::unique_ptr<PendingCheck> check,
    FullHashToStoreAndHashPrefixesMap results) {
  if (GetPrefixMatchesIsAsync()) {
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
  }

  if (results.empty()) {
    RespondToClient(std::move(check));
  } else {
    check->full_hash_to_store_and_hash_prefixes = results;
    AddPendingCheck(check.get());
    PerformFullHashCheck(std::move(check));
  }
}

void V4LocalDatabaseManager::RespondSafeToQueuedAndPendingChecks() {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

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
    if (it->client_callback_type == ClientCallbackType::CHECK_OTHER &&
        GetPrefixMatchesIsAsync()) {
      // In this case there's a callback that will run when weak_factory_ is
      // invalidated.
      continue;
    }
    // We don't own the unique pointer for the pending check, so we do not
    // perform cleanup on it while responding to the client.
    RespondToClientWithoutPendingCheckCleanup(it);
  }
}

void V4LocalDatabaseManager::DropQueuedAndPendingChecks() {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  queued_checks_.clear();
  // Intentionally ignore the checks this method returns
  CopyAndRemoveAllPendingChecks();
}

void V4LocalDatabaseManager::RespondToClient(
    std::unique_ptr<PendingCheck> check) {
  RespondToClientWithoutPendingCheckCleanup(check.get());
}
void V4LocalDatabaseManager::RespondToClientWithoutPendingCheckCleanup(
    PendingCheck* check) {
  DCHECK(check);

  switch (check->client_callback_type) {
    case ClientCallbackType::CHECK_BROWSE_URL:
    case ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckBrowseUrlResult(
          check->urls[0], check->most_severe_threat_type, check->url_metadata);
      break;

    case ClientCallbackType::CHECK_DOWNLOAD_URLS:
      check->client->OnCheckDownloadUrlResult(check->urls,
                                              check->most_severe_threat_type);
      break;

    case ClientCallbackType::CHECK_RESOURCE_URL:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckResourceUrlResult(check->urls[0],
                                              check->most_severe_threat_type,
                                              check->matching_full_hash);
      break;

    case ClientCallbackType::CHECK_CSD_ALLOWLIST: {
      DCHECK_EQ(1u, check->urls.size());
      bool did_match_allowlist =
          check->most_severe_threat_type == SB_THREAT_TYPE_CSD_ALLOWLIST;
      DCHECK(did_match_allowlist ||
             check->most_severe_threat_type == SB_THREAT_TYPE_SAFE);
      check->client->OnCheckAllowlistUrlResult(did_match_allowlist);
      break;
    }

    case ClientCallbackType::CHECK_EXTENSION_IDS: {
      DCHECK_EQ(check->full_hash_threat_types.size(),
                check->full_hashes.size());
      std::set<FullHashStr> unsafe_extension_ids;
      for (size_t i = 0; i < check->full_hash_threat_types.size(); i++) {
        if (check->full_hash_threat_types[i] == SB_THREAT_TYPE_EXTENSION) {
          unsafe_extension_ids.insert(check->full_hashes[i]);
        }
      }
      check->client->OnCheckExtensionsResult(unsafe_extension_ids);
      break;
    }

    case ClientCallbackType::CHECK_OTHER:
      NOTREACHED() << "Unexpected client_callback_type encountered";
  }
}

void V4LocalDatabaseManager::SetupDatabase() {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  // Do not create the database on the SB thread since this may be an expensive
  // operation. Instead, do that on the task_runner and when the new database
  // has been created, swap it out on the SB thread.
  NewDatabaseReadyCallback db_ready_callback =
      base::BindOnce(&V4LocalDatabaseManager::DatabaseReadyForChecks,
                     weak_factory_.GetWeakPtr());
  V4Database::Create(task_runner_, base_path_, list_infos_,
                     std::move(db_ready_callback));
}

void V4LocalDatabaseManager::SetupUpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  V4UpdateCallback update_callback =
      base::BindRepeating(&V4LocalDatabaseManager::UpdateRequestCompleted,
                          weak_factory_.GetWeakPtr());

  v4_update_protocol_manager_ = V4UpdateProtocolManager::Create(
      url_loader_factory, config, update_callback,
      extended_reporting_level_callback_);
}

void V4LocalDatabaseManager::UpdateRequestCompleted(
    std::unique_ptr<ParsedServerResponse> parsed_server_response) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            db_updated_callback_);
}

bool V4LocalDatabaseManager::AreAllStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return IsDatabaseReady() &&
         v4_database_->AreAllStoresAvailable(stores_to_check);
}

int64_t V4LocalDatabaseManager::GetStoreEntryCount(const ListIdentifier& store,
                                                   int bytes_per_entry) const {
  if (!IsDatabaseReady()) {
    return 0;
  }
  return v4_database_->GetStoreSizeInBytes(store) / bytes_per_entry;
}

bool V4LocalDatabaseManager::IsStoreTooSmall(const ListIdentifier& store,
                                             int bytes_per_entry,
                                             int min_entry_count) const {
  return GetStoreEntryCount(store, bytes_per_entry) < min_entry_count;
}

bool V4LocalDatabaseManager::AreAnyStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return IsDatabaseReady() &&
         v4_database_->AreAnyStoresAvailable(stores_to_check);
}

void V4LocalDatabaseManager::UpdateListClientStates(
    const std::unique_ptr<StoreStateMap>& store_state_map) {
  list_client_states_.clear();
  V4ProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
      store_state_map, &list_client_states_);
}

void V4LocalDatabaseManager::AddPendingCheck(PendingCheck* check) {
  check->is_in_pending_checks = true;
  pending_checks_.insert(check);
}

void V4LocalDatabaseManager::RemovePendingCheck(
    PendingChecks::const_iterator it) {
  (*it)->is_in_pending_checks = false;
  pending_checks_.erase(it);
}

V4LocalDatabaseManager::PendingChecks
V4LocalDatabaseManager::CopyAndRemoveAllPendingChecks() {
  PendingChecks pending_checks;
  pending_checks.swap(pending_checks_);
  for (PendingCheck* check : pending_checks) {
    check->is_in_pending_checks = false;
  }
  return pending_checks;
}

void V4LocalDatabaseManager::DeleteUnusedStoreFiles() {
  for (auto* const store_filename_to_delete : kStoreFileNamesToDelete) {
    // Is the file marked for deletion also being used for a valid V4Store?
    auto it = std::find_if(std::begin(list_infos_), std::end(list_infos_),
                           [&store_filename_to_delete](ListInfo const& li) {
                             return li.filename() == store_filename_to_delete;
                           });
    if (list_infos_.end() == it) {
      const base::FilePath store_path =
          base_path_.AppendASCII(store_filename_to_delete);
      base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                                 base::BindOnce(&MaybeDeleteStore, store_path));
    } else {
      NOTREACHED() << "Trying to delete a store file that's in use: "
                   << store_filename_to_delete;
    }
  }
}

}  // namespace safe_browsing
