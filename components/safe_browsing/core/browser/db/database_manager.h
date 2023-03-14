// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_DATABASE_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// Value returned by some functions that check an allowlist and may or may not
// have an immediate answer.
enum class AsyncMatch : int {
  // If a hash prefix on the allowlist matches any of the computed hashes for
  // the URL. In this case, the callback method on the client is called back
  // later with the result.
  ASYNC,

  // If a full hash on the allowlist matches any of the computed hashes for the
  // URL. The callback function isn't called.
  MATCH,

  // If Safe Browsing isn't enabled, or the allowlist hasn't been sync'd yet, or
  // when no hash prefix or full hash in the allowlist matches the computed
  // hashes of the URL. The callback function isn't called.
  NO_MATCH,

  kMaxValue = NO_MATCH,
};

struct V4ProtocolConfig;
class V4GetHashProtocolManager;

// Base class to either the locally-managed or a remotely-managed database.
class SafeBrowsingDatabaseManager
    : public base::RefCountedDeleteOnSequence<SafeBrowsingDatabaseManager> {
 public:
  // Callers requesting a result should derive from this class.
  // The destructor should call db_manager->CancelCheck(client) if a
  // request is still pending.
  class Client {
   public:
    virtual ~Client() {}

    // Called when the result of checking the API blocklist is known.
    // TODO(kcarattini): Consider if we need |url| passed here, remove if not.
    virtual void OnCheckApiBlocklistUrlResult(const GURL& url,
                                              const ThreatMetadata& metadata) {}

    // Called when the result of checking a browse URL is known or the result of
    // checking the URL for subresource filter is known.
    virtual void OnCheckBrowseUrlResult(const GURL& url,
                                        SBThreatType threat_type,
                                        const ThreatMetadata& metadata) {}

    // Called when the result of checking a download URL is known.
    virtual void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                          SBThreatType threat_type) {}

    // Called when the result of checking a set of extensions is known.
    virtual void OnCheckExtensionsResult(const std::set<std::string>& threats) {
    }

    // Called when the result of checking the resource blocklist is known.
    virtual void OnCheckResourceUrlResult(const GURL& url,
                                          SBThreatType threat_type,
                                          const std::string& threat_hash) {}

    // Called when the result of checking a allowlist is known.
    // Currently only used for CSD allowlist.
    virtual void OnCheckAllowlistUrlResult(bool did_match_allowlist) {}
  };

  //
  // Methods called by the client to cancel pending checks.
  //

  // Called on the IO thread to cancel a pending API check if the result is no
  // longer needed. Returns true if the client was found and the check
  // successfully cancelled.
  virtual bool CancelApiCheck(Client* client);

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.  Also called after the result has been handled. Api checks
  // are handled separately. To cancel an API check use CancelApiCheck.
  // If |client| doesn't exist anymore, ignore this call.
  virtual void CancelCheck(Client* client) = 0;

  //
  // Methods to check whether the database manager supports a certain feature.
  //

  // Returns true if this request destination should be checked.
  virtual bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const = 0;

  // Returns true if the url's scheme can be checked.
  virtual bool CanCheckUrl(const GURL& url) const = 0;

  // Returns true if checks are never done synchronously, and therefore
  // always have some latency.
  virtual bool ChecksAreAlwaysAsync() const = 0;

  //
  // Methods to check (possibly asynchronously) whether a given resource is
  // safe. If the database manager can't determine it synchronously, the
  // appropriate method on the |client| is called back when the reputation of
  // the resource is known.
  //

  // Called on the IO thread to check if the given url has blocklisted APIs.
  // |client| is called asynchronously with the result when it is ready. Callers
  // should wait for results before calling this method a second time with the
  // same client. This method has the same implementation for both the local and
  // remote database managers since it pings Safe Browsing servers directly
  // without accessing the database at all.  Returns true if we can
  // synchronously determine that the url is safe. Otherwise it returns false,
  // and |client| is called asynchronously with the result when it is ready.
  virtual bool CheckApiBlocklistUrl(const GURL& url, Client* client);

  // Check if the |url| matches any of the full-length hashes from the client-
  // side phishing detection allowlist. The 3-state return value indicates
  // the result or that |client| will get a callback later with the result.
  virtual AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) = 0;

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and |client| is called asynchronously with the
  // result when it is ready. The URL will only be checked for the threat types
  // in |threat_types|. |experiment_cache_selection| specifies which cache to
  // use. See comments above MechanismExperimentHashDatabaseCache's definition
  // for more details.
  virtual bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection) = 0;

  // Check if the prefix for |url| is in safebrowsing download add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                                Client* client) = 0;

  // Check which prefixes in |extension_ids| are in the safebrowsing blocklist.
  // Returns true if not, false if further checks need to be made in which case
  // the result will be passed to |client|.
  virtual bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                                 Client* client) = 0;

  // Check if |url| is in the resources blocklist. Returns true if not, false
  // if further checks need to be made in which case the result will be passed
  // to callback in |client|.
  virtual bool CheckResourceUrl(const GURL& url, Client* client) = 0;

  // Called on the IO thread to check if the given url belongs to a list the
  // subresource cares about. If the url doesn't belong to any such list and the
  // check can happen synchronously, returns true. Otherwise it returns false,
  // and |client| is called asynchronously with the result when it is ready.
  // Returns true if the list is not yet available.
  virtual bool CheckUrlForSubresourceFilter(const GURL& url,
                                            Client* client) = 0;

  // Called on the IO thread to check whether |url| is safe by checking if it
  // appears on a high-confidence allowlist. The return value is true if it
  // matches the allowlist, and is false if it does not. The high confidence
  // allowlist is a list of full hashes of URLs that are expected to be safe so
  // in the case of a match on this list, the realtime full URL Safe Browsing
  // lookup isn't performed. |metric_variation| is used for logging purposes to
  // specify the consumer mechanism performing this check in histograms.
  virtual bool CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      const std::string& metric_variation) = 0;

  //
  // Match*(): Methods to synchronously check if various types are safe.
  //

  // Check if the |url| matches any of the full-length hashes from the download
  // allowlist.  Returns true if there was a match and false otherwise. To make
  // sure we are conservative we will return true if an error occurs.  This
  // method must be called on the IO thread.
  virtual bool MatchDownloadAllowlistUrl(const GURL& url) = 0;

  // Check if the given IP address (either IPv4 or IPv6) matches the malware
  // IP blocklist.
  virtual bool MatchMalwareIP(const std::string& ip_address) = 0;

  //
  // Methods to check the config of the DatabaseManager.
  //

  // Returns the lists that this DatabaseManager should get full hashes for.
  virtual StoresToCheck GetStoresForFullHashRequests();

  // Returns the client_state of each of the lists that this DatabaseManager
  // syncs.
  virtual std::unique_ptr<StoreStateMap> GetStoreStateMap();

  // Returns the ThreatSource for this implementation.
  virtual ThreatSource GetThreatSource() const = 0;

  // Returns whether download protection is enabled.
  virtual bool IsDownloadProtectionEnabled() const = 0;

  // Calls the method with the same name in |v4_get_hash_protocol_manager_|.
  virtual void SetLookupMechanismExperimentIsEnabled();

  //
  // Methods to indicate when to start or suspend the SafeBrowsing operations.
  // These functions are always called on the IO thread.
  //

  // Called to initialize objects that are used on the io_thread, such as the
  // v4 protocol manager.  This may be called multiple times during the life of
  // the DatabaseManager. Must be called on IO thread unless
  // kSafeBrowsingOnUIThread is enabled in which case it'll be UI thread. All
  // subclasses should override this method, set enabled_ to true and call the
  // base class method at the top of it.
  virtual void StartOnSBThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config);

  //
  // Method to manage getting database updates of the DatabaseManager.
  //

  // Subscribe to receive callbacks when the database is updated, both initially
  // when it's loaded from disk at startup, and then periodically. These
  // callbacks will be on the UI thread.
  using OnDatabaseUpdated = base::RepeatingClosure;
  base::CallbackListSubscription RegisterDatabaseUpdatedCallback(
      const OnDatabaseUpdated& cb);

  // Called to stop or shutdown operations on the io_thread unless
  // kSafeBrowsingOnUIThread is enabled in which case it'll be UI thread. All
  // subclasses should override this method, set enabled_ to false and call the
  // base class method at the bottom of it.
  virtual void StopOnSBThread(bool shutdown);

  // Called to check if database is ready or not.
  virtual bool IsDatabaseReady();

 protected:
  // Bundled client info for an API abuse hash prefix check.
  class SafeBrowsingApiCheck {
   public:
    SafeBrowsingApiCheck(const GURL& url, Client* client);

    SafeBrowsingApiCheck(const SafeBrowsingApiCheck&) = delete;
    SafeBrowsingApiCheck& operator=(const SafeBrowsingApiCheck&) = delete;

    ~SafeBrowsingApiCheck() = default;

    const GURL& url() const { return url_; }
    Client* client() const { return client_; }

   private:
    GURL url_;

    // Not owned.
    raw_ptr<Client> client_;
  };

  SafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  virtual ~SafeBrowsingDatabaseManager();

  friend class base::RefCountedDeleteOnSequence<SafeBrowsingDatabaseManager>;
  friend class base::DeleteHelper<SafeBrowsingDatabaseManager>;
  friend class V4LocalDatabaseManager;

  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           CheckApiBlocklistUrlPrefixes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResults);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResultsNoMatch);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResultsMatches);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest, CancelApiCheck);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest, ResultsAreCached);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           ResultsAreNotCachedOnNull);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest, GetCachedResults);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           CachedResultsMerged);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           CachedResultsAreEvicted);

  // Called on the IO thread when the SafeBrowsingProtocolManager has received
  // the full hash and api results for prefixes of the |url| argument in
  // CheckApiBlocklistUrl.
  void OnThreatMetadataResponse(std::unique_ptr<SafeBrowsingApiCheck> check,
                                const ThreatMetadata& md);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  // SafeBrowsingDatabaseManager passes its |io_task_runner| construction
  // parameter to its RefCountedDeleteOnSequence base class, which exposes its
  // passed-in task runner as owning_task_runner(). Expose that |io_task_runner|
  // parameter internally as io_task_runner() for clarity.
  // Note if kSafeBrowsingOnUIThread is enabled that'll be the UI thread.
  scoped_refptr<base::SequencedTaskRunner> sb_task_runner() {
    return owning_task_runner();
  }

  typedef std::set<SafeBrowsingApiCheck*> ApiCheckSet;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // In-progress checks. This set owns the SafeBrowsingApiCheck pointers and is
  // responsible for deleting them when removing from the set.
  ApiCheckSet api_checks_;

  // Whether the service is running. 'enabled_' is used by the
  // SafeBrowsingDatabaseManager on the IO thread during normal operations.
  bool enabled_;

  // Make callbacks about the completion of database update process. This is
  // currently used by the extension blocklist checker to disable any installed
  // extensions that have been blocklisted since.
  void NotifyDatabaseUpdateFinished();

  // Created and destroyed via StartOnSBThread/StopOnSBThread.
  std::unique_ptr<V4GetHashProtocolManager> v4_get_hash_protocol_manager_;

  // A list of parties to be notified about database updates.
  base::RepeatingClosureList update_complete_callback_list_;

 private:
  // Returns an iterator to the pending API check with the given |client|.
  ApiCheckSet::iterator FindClientApiCheck(Client* client);

};  // class SafeBrowsingDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_DATABASE_MANAGER_H_
