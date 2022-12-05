// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_LOCAL_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_LOCAL_DATABASE_MANAGER_H_

// A class that provides the interface between the SafeBrowsing protocol manager
// and database that holds the downloaded updates.

#include <memory>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/v4_database.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_update_protocol_manager.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace safe_browsing {

typedef unsigned ThreatSeverity;

// Manages the local, on-disk database of updates downloaded from the
// SafeBrowsing service and interfaces with the protocol manager.
class V4LocalDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Create and return an instance of V4LocalDatabaseManager, if Finch trial
  // allows it; nullptr otherwise.
  static scoped_refptr<V4LocalDatabaseManager> Create(
      const base::FilePath& base_path,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      ExtendedReportingLevelCallback extended_reporting_level_callback);

  V4LocalDatabaseManager(const V4LocalDatabaseManager&) = delete;
  V4LocalDatabaseManager& operator=(const V4LocalDatabaseManager&) = delete;

  // Populates the protobuf with the database data.
  void CollectDatabaseManagerInfo(
      DatabaseManagerInfo* v4_database_info,
      FullHashCacheInfo* full_hash_cache_info) const;

  // Return an instance of the V4LocalDatabaseManager object
  static const V4LocalDatabaseManager* current_local_database_manager() {
    return current_local_database_manager_;
  }

  //
  // SafeBrowsingDatabaseManager implementation
  //

  void CancelCheck(Client* client) override;
  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(const GURL& url,
                      const SBThreatTypeSet& threat_types,
                      Client* client) override;
  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  // TODO(vakh): |CheckExtensionIDs| in the base class accepts a set of
  // std::strings but the overriding method in this class accepts a set of
  // FullHash objects. Since FullHash is currently std::string, it compiles,
  // but this difference should be eliminated.
  bool CheckExtensionIDs(const std::set<FullHash>& extension_ids,
                         Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool CheckUrlForHighConfidenceAllowlist(const GURL& url) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool MatchDownloadAllowlistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool IsDownloadProtectionEnabled() const override;

  void StartOnIOThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;

  // The stores/lists to always get full hashes for, regardless of which store
  // the hash prefix matched. We request all lists since it makes the full hash
  // cache management simpler and we expect very few lists to have overlap for
  // the same hash prefix anyway.
  StoresToCheck GetStoresForFullHashRequests() override;

  std::unique_ptr<StoreStateMap> GetStoreStateMap() override;

  //
  // End: SafeBrowsingDatabaseManager implementation
  //

 protected:
  // Construct V4LocalDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  V4LocalDatabaseManager(
      const base::FilePath& base_path,
      ExtendedReportingLevelCallback extended_reporting_level_callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_tests);

  ~V4LocalDatabaseManager() override;

  enum class ClientCallbackType : int {
    // This represents the case when we're trying to determine if a URL is
    // unsafe from the following perspectives: Malware, Phishing, UwS.
    CHECK_BROWSE_URL,

    // This represents the case when we're trying to determine if any of the
    // URLs in a vector of URLs is unsafe for downloading binaries.
    CHECK_DOWNLOAD_URLS,

    // This represents the case when we're trying to determine if a URL is an
    // unsafe resource.
    CHECK_RESOURCE_URL,

    // This represents the case when we're trying to determine if a Chrome
    // extension is a unsafe.
    CHECK_EXTENSION_IDS,

    // This respresents the case when we're trying to determine if a URL belongs
    // to the list where subresource filter should be active.
    CHECK_URL_FOR_SUBRESOURCE_FILTER,

    // This respresents the case when we're trying to determine if a URL is
    // part of the CSD allowlist.
    CHECK_CSD_ALLOWLIST,

    // This represents the other cases when a check is being performed
    // synchronously so a client callback isn't required. For instance, when
    // trying to determing if an IP address is unsafe due to hosting Malware.
    CHECK_OTHER,
  };

  // The information we need to process a URL safety reputation request and
  // respond to the SafeBrowsing client that asked for it.
  struct PendingCheck {
    PendingCheck(Client* client,
                 ClientCallbackType client_callback_type,
                 const StoresToCheck& stores_to_check,
                 const std::vector<GURL>& urls);

    PendingCheck(Client* client,
                 ClientCallbackType client_callback_type,
                 const StoresToCheck& stores_to_check,
                 const std::set<FullHash>& full_hashes);

    ~PendingCheck();

    // The SafeBrowsing client that's waiting for the safe/unsafe verdict.
    raw_ptr<Client, DanglingUntriaged> client;

    // Determines which funtion from the |client| needs to be called once we
    // know whether the URL in |url| is safe or unsafe.
    const ClientCallbackType client_callback_type;

    // The most severe threat verdict for the URLs/hashes being checked.
    SBThreatType most_severe_threat_type;

    // When the check was sent to the SafeBrowsing service. Used to record the
    // time it takes to get the uncached full hashes from the service (or a
    // cached full hash response).
    base::TimeTicks full_hash_check_start;

    // The SafeBrowsing lists to check hash prefixes in.
    const StoresToCheck stores_to_check;

    // The URLs that are being checked for being unsafe. The size of exactly
    // one of |full_hashes| and |urls| should be greater than 0.
    const std::vector<GURL> urls;

    // The full hashes that are being checked for being safe.
    std::vector<FullHash> full_hashes;

    // The most severe SBThreatType for each full hash in |full_hashes|. The
    // length of |full_hash_threat_type| must always match |full_hashes|.
    std::vector<SBThreatType> full_hash_threat_types;

    // List of full hashes of urls we are checking and corresponding store and
    // hash prefixes that match it in the local database.
    FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;

    // List of full hashes of urls we are checking and corresponding store and
    // hash prefixes that match it in the artificial database.
    FullHashToStoreAndHashPrefixesMap
        artificial_full_hash_to_store_and_hash_prefixes;

    // The metadata associated with the full hash of the severest match found
    // for that URL.
    ThreatMetadata url_metadata;

    // The full hash that matched for a blocklisted resource URL. Used only for
    // |CheckResourceUrl| case.
    FullHash matching_full_hash;

    // Specifies whether the PendingCheck is in the V4LocalDatabaseManager's
    // |pending_checks_| set. This property is for sanity-checking that when the
    // check is destructed, it should never still be in |pending_checks_|, since
    // functions could still be called on those checks afterwards.
    bool is_in_pending_checks = false;
  };

  typedef std::vector<std::unique_ptr<PendingCheck>> QueuedChecks;

 private:
  friend class V4LocalDatabaseManagerTest;
  friend class FakeV4LocalDatabaseManager;
  FRIEND_TEST_ALL_PREFIXES(V4LocalDatabaseManagerTest,
                           TestGetSeverestThreatTypeAndMetadata);
  FRIEND_TEST_ALL_PREFIXES(V4LocalDatabaseManagerTest, NotificationOnUpdate);
  FRIEND_TEST_ALL_PREFIXES(V4LocalDatabaseManagerTest, SyncedLists);

  // The checks awaiting a full hash response from SafeBrowsing service.
  typedef std::unordered_set<PendingCheck*> PendingChecks;

  // Called when all the stores managed by the database have been read from
  // disk after startup and the database is ready for checking resource
  // reputation.
  void DatabaseReadyForChecks(
      std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database);

  // Called when all the stores managed by the database have been verified for
  // checksum correctness after startup and the database is ready for applying
  // updates.
  void DatabaseReadyForUpdates(
      const std::vector<ListIdentifier>& stores_to_reset);

  // Called when the database has been updated and schedules the next update.
  void DatabaseUpdated();

  // Matches the full_hashes for a |check| with the hashes stored in
  // |artificially_marked_store_and_hash_prefixes_|. For each full hash match,
  // it populates |full_hash_to_store_and_hash_prefixes| with the matched hash
  // prefix and store.
  void GetArtificialPrefixMatches(const std::unique_ptr<PendingCheck>& check);

  // Identifies the prefixes and the store they matched in, for a given |check|.
  // Returns true if one or more hash prefix matches are found; false otherwise.
  bool GetPrefixMatches(const std::unique_ptr<PendingCheck>& check);

  // Goes over the |full_hash_infos| and stores the most severe SBThreatType in
  // |most_severe_threat_type|, the corresponding metadata in |metadata|, and
  // the matching full hash in |matching_full_hash|. Also, updates in
  // |full_hash_threat_types|, the threat type for each full hash in
  // |full_hashes|.
  void GetSeverestThreatTypeAndMetadata(
      const std::vector<FullHashInfo>& full_hash_infos,
      const std::vector<FullHash>& full_hashes,
      std::vector<SBThreatType>* full_hash_threat_types,
      SBThreatType* most_severe_threat_type,
      ThreatMetadata* metadata,
      FullHash* matching_full_hash);

  // Returns the SBThreatType for a given ListIdentifier.
  SBThreatType GetSBThreatTypeForList(const ListIdentifier& list_id);

  // Queues the check for async response if the database isn't ready yet.
  // If the database is ready, checks the database for prefix matches and
  // returns true immediately if there's no match. If a match is found, it
  // schedules a task to perform full hash check and returns false.
  bool HandleCheck(std::unique_ptr<PendingCheck> check);

  // Like HandleCheck, but for allowlists that have both full-hashes and
  // partial hashes in the DB. If |allow_async_check| is false, it will only
  // return either MATCH or NO_MATCH. If |allow_async_check| is true, it returns
  // MATCH, NO_MATCH, or ASYNC. In the ASYNC case, it will schedule performing
  // the full hash check.
  AsyncMatch HandleAllowlistCheck(std::unique_ptr<PendingCheck> check,
                                  bool allow_async_check);

  // Computes the hashes of URLs that have artificially been marked as unsafe
  // using any of the following command line flags: "mark_as_phishing",
  // "mark_as_malware", "mark_as_uws".
  void PopulateArtificialDatabase();

  // Schedules a full-hash check for a given set of prefixes.
  void ScheduleFullHashCheck(std::unique_ptr<PendingCheck> check);

  // Checks |stores_to_check| in database synchronously for hash prefixes
  // matching |hash|. Returns true if there's a match; false otherwise. This is
  // used for lists that have full hash information in the database.
  bool HandleHashSynchronously(const FullHash& hash,
                               const StoresToCheck& stores_to_check);

  // Checks |stores_to_check| in database synchronously for hash prefixes
  // matching the full hashes for |url|. See |HandleHashSynchronously| for
  // details.
  bool HandleUrlSynchronously(const GURL& url,
                              const StoresToCheck& stores_to_check);

  // Called when the |v4_get_hash_protocol_manager_| has the full hash response
  // available for the URL that we requested. It determines the severest
  // threat type and responds to the |client| with that information.
  virtual void OnFullHashResponse(
      std::unique_ptr<PendingCheck> pending_check,
      const std::vector<FullHashInfo>& full_hash_infos);

  // Performs the full hash checking of the URL in |check|.
  virtual void PerformFullHashCheck(std::unique_ptr<PendingCheck> check);

  // When the database is ready to use, process the checks that were queued
  // while the database was loading from disk.
  void ProcessQueuedChecks();

  // Called on StopOnIOThread, it responds to the clients that are (1) waiting
  // for the database to become available with the verdict as SAFE, or (2)
  // waiting for a full hash response from the SafeBrowsing service.
  void RespondSafeToQueuedAndPendingChecks();

  // Calls the appopriate method on the |client| object, based on the contents
  // of |pending_check|.
  void RespondToClient(std::unique_ptr<PendingCheck> pending_check);

  // Callers should generally use |RespondToClient| instead, which will clean up
  // the |pending_check|. Callers should use this function when they don't own
  // the |pending_check|. Like |RespondToClient|, this calls the appropriate
  // method on the |client| object, based on the contents of |pending_check|.
  void RespondToClientWithoutPendingCheckCleanup(PendingCheck* pending_check);

  // Instantiates and initializes |v4_database_| on the task runner. Sets up the
  // callback for |DatabaseReady| when the database is ready for use.
  void SetupDatabase();

  // Instantiates and initializes |v4_update_protocol_manager_|.
  void SetupUpdateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config);

  // Updates the |list_client_states_| with the state information in
  // |store_state_map|.
  void UpdateListClientStates(
      const std::unique_ptr<StoreStateMap>& store_state_map);

  // The callback called each time the protocol manager downloads updates
  // successfully.
  void UpdateRequestCompleted(
      std::unique_ptr<ParsedServerResponse> parsed_server_response);

  // Return true if we're enabled and have loaded real data for all of
  // these stores.
  bool AreAllStoresAvailableNow(const StoresToCheck& stores_to_check) const;

  // Return the number of entries in the store. If the database isn't enabled or
  // the database is not found, return 0.
  int64_t GetStoreEntryCount(const ListIdentifier& store,
                             int bytes_per_entry) const;

  // Return whether the size of the store is smaller than expected.
  bool IsStoreTooSmall(const ListIdentifier& store,
                       int bytes_per_entry,
                       int min_entry_count) const;

  // Return true if we're enabled and have loaded real data for any of
  // these stores.
  bool AreAnyStoresAvailableNow(const StoresToCheck& stores_to_check) const;

  // Starts tracking a check that is awaiting a full hash response from the
  // SafeBrowsing service.
  void AddPendingCheck(PendingCheck* check);

  // Stops tracking a check that is awaiting a full hash response from the
  // SafeBrowsing service.
  void RemovePendingCheck(PendingChecks::const_iterator it);

  // Stops tracking all checks awaiting a full hash response from the
  // SafeBrowsing service. Returns the swapped copy of the checks.
  PendingChecks CopyAndRemoveAllPendingChecks();

  // Stores full hashes of URLs that have been artificially marked as unsafe.
  StoreAndHashPrefixes artificially_marked_store_and_hash_prefixes_;

  // The base directory under which to create the files that contain hashes.
  const base::FilePath base_path_;

  // Instance of the V4LocalDatabaseManager object
  static const V4LocalDatabaseManager* current_local_database_manager_;

  // Called when the V4Database has finished applying the latest update and is
  // ready to process next update.
  DatabaseUpdatedCallback db_updated_callback_;

  // Callback to get the current extended reporting level. Needed by the update
  // manager.
  ExtendedReportingLevelCallback extended_reporting_level_callback_;

  // The client_state of each list currently being synced. This is updated each
  // time a database update completes, and used to send list client_state
  // information in the full hash request.
  std::vector<std::string> list_client_states_;

  // The list of stores to manage (for hash prefixes and full hashes). Each
  // element contains the identifier for the store, the corresponding
  // SBThreatType, whether to fetch hash prefixes for that store, and the
  // name of the file on disk that would contain the prefixes, if applicable.
  ListInfos list_infos_;

  // The checks awaiting for a full hash response from the SafeBrowsing service.
  // These are used to avoid responding to a client if it cancels a pending
  // check, and to respond back "safe" to all waiting clients if SafeBrowsing is
  // stopped.
  PendingChecks pending_checks_;

  // The checks that need to be scheduled when the database becomes ready for
  // use.
  QueuedChecks queued_checks_;

  // The sequenced task runner for running safe browsing database operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The database that manages the stores containing the hash prefix updates.
  // All writes to this variable must happen on the IO thread only.
  std::unique_ptr<V4Database, base::OnTaskRunnerDeleter> v4_database_;

  // The protocol manager that downloads the hash prefix updates.
  std::unique_ptr<V4UpdateProtocolManager> v4_update_protocol_manager_;

  base::WeakPtrFactory<V4LocalDatabaseManager> weak_factory_{this};
};  // class V4LocalDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_LOCAL_DATABASE_MANAGER_H_
