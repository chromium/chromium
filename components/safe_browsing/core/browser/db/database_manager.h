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
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
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
    Client();
    virtual ~Client();

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

    // Called when the result of checking a allowlist is known.
    // Currently only used for CSD allowlist.
    virtual void OnCheckAllowlistUrlResult(bool did_match_allowlist) {}

    // Returns a WeakPtr to this.
    base::WeakPtr<Client> GetWeakPtr();

   private:
    base::WeakPtrFactory<Client> weak_factory_{this};
  };

  //
  // Methods called by the client to cancel pending checks.
  //

  // Cancels a pending API check if the result is no longer needed. Returns true
  // if the client was found and the check successfully cancelled. This should
  // be called on the UI thread.
  virtual bool CancelApiCheck(Client* client);

  // Cancels a pending check if the result is no longer needed.  Also called
  // after the result has been handled. Api checks are handled separately. To
  // cancel an API check use CancelApiCheck. If |client| doesn't exist anymore,
  // ignore this call. This should be called on the UI thread.
  virtual void CancelCheck(Client* client) = 0;

  //
  // Methods to check whether the database manager supports a certain feature.
  //

  // Returns true if the url's scheme can be checked.
  virtual bool CanCheckUrl(const GURL& url) const = 0;

  //
  // Methods to check (possibly asynchronously) whether a given resource is
  // safe. If the database manager can't determine it synchronously, the
  // appropriate method on the |client| is called back when the reputation of
  // the resource is known.
  //

  // Checks if the given url has blocklisted APIs. |client| is called
  // asynchronously with the result when it is ready. Callers should wait for
  // results before calling this method a second time with the same client. This
  // method has the same implementation for both the local and remote database
  // managers since it pings Safe Browsing servers directly without accessing
  // the database at all.  Returns true if we can synchronously determine that
  // the url is safe. Otherwise it returns false, and |client| is called
  // asynchronously with the result when it is ready. This should be called on
  // the UI thread.
  virtual bool CheckApiBlocklistUrl(const GURL& url, Client* client);

  // Check if the |url| matches any of the full-length hashes from the client-
  // side phishing detection allowlist. The 3-state return value indicates
  // the result or that |client| will get a callback later with the result.
  virtual AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) = 0;

  // Checks if the given url is safe or not.  If we can synchronously determine
  // that the url is safe, CheckUrl returns true. Otherwise it returns false,
  // and |client| is called asynchronously with the result when it is ready. The
  // URL will only be checked for the threat types in |threat_types|.
  // |check_type| specifies the type of check the url will be checked against.
  // See comments above CheckBrowseUrlType's definition for more details. This
  // should be called on the UI thread.
  virtual bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      CheckBrowseUrlType check_type) = 0;

  // Check if the prefix for |url| is in safebrowsing download add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                                Client* client) = 0;

  // Check which prefixes in |extension_ids| are in the safebrowsing blocklist.
  // Returns true if not, false if further checks need to be made in which case
  // the result will be passed to |client|.
  virtual bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                                 Client* client) = 0;

  // Checks if the given url belongs to a list the subresource cares about. If
  // the url doesn't belong to any such list and the check can happen
  // synchronously, returns true. Otherwise it returns false, and |client| is
  // called asynchronously with the result when it is ready. Returns true if the
  // list is not yet available. This should be called on the UI thread.
  virtual bool CheckUrlForSubresourceFilter(const GURL& url,
                                            Client* client) = 0;

  // Passed to CheckUrlForHighConfidenceAllowlist() callback. Should be used for
  // logging purposes only.
  struct HighConfidenceAllowlistCheckLoggingDetails {
    // Whether the database stores were available when the check ran.
    bool were_all_stores_available = false;
    // Whether the allowlist store was too small when the check ran.
    bool was_allowlist_size_too_small = false;
  };

  using CheckUrlForHighConfidenceAllowlistCallback = base::OnceCallback<void(
      bool url_on_high_confidence_allowlist,
      std::optional<HighConfidenceAllowlistCheckLoggingDetails>)>;

  // Checks whether |url| is safe by checking if it appears on a high-confidence
  // allowlist. `callback` is run asynchronously with true if it matches the
  // allowlist, and is false if it does not. The high confidence allowlist is a
  // list of full hashes of URLs that are expected to be safe so in the case of
  // a match on this list, the realtime full URL Safe Browsing lookup isn't
  // performed. The returned value includes some details about the store state
  // when the call was made. If used, it should be used for logging purposes
  // only. This should be called on the UI thread.
  virtual void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) = 0;

  //
  // Match*(): Methods to synchronously check if various types are safe.
  //

  // Check if the |url| matches any of the full-length hashes from the download
  // allowlist. Runs `callback` asynchronously with true if there was a match
  // and false otherwise. To make sure we are conservative we will return true
  // if an error occurs. This should be called on the UI thread.
  virtual void MatchDownloadAllowlistUrl(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) = 0;

  //
  // Methods to check the config of the DatabaseManager.
  //

  // Returns the lists that this DatabaseManager should get full hashes for.
  virtual StoresToCheck GetStoresForFullHashRequests();

  // Returns the client_state of each of the lists that this DatabaseManager
  // syncs.
  virtual std::unique_ptr<StoreStateMap> GetStoreStateMap();

  // Returns the ThreatSource of browse URL check (i.e. URLs checked by the
  // |CheckBrowseUrl| function) for this implementation.
  virtual ThreatSource GetBrowseUrlThreatSource(
      CheckBrowseUrlType check_type) const = 0;

  // Returns the ThreatSource of non-browse URL check (i.e. URLs or other
  // entities that are not checked by the |CheckBrowseUrl| function) for this
  // implementation.
  virtual ThreatSource GetNonBrowseUrlThreatSource() const = 0;

  //
  // Methods to indicate when to start or suspend the SafeBrowsing operations.
  // These functions should be called on the UI thread.
  //

  // Called to initialize objects that are used on the thread, such as the v4
  // protocol manager.  This may be called multiple times during the life of the
  // DatabaseManager. All subclasses should override this method and call the
  // base class method at the top of it. This should be called on the UI thread.
  virtual void StartOnUIThread(
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

  // Called to stop or shutdown operations. All subclasses should override this
  // method and call the base class method at the bottom of it. This should be
  // called on the UI thread.
  virtual void StopOnUIThread(bool shutdown);

  // Called to check if database is ready or not.
  virtual bool IsDatabaseReady() const = 0;

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

  explicit SafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

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

  // Called when the SafeBrowsingProtocolManager has received the full hash and
  // api results for prefixes of the |url| argument in CheckApiBlocklistUrl.
  // This should be called on the UI thread.
  void OnThreatMetadataResponse(std::unique_ptr<SafeBrowsingApiCheck> check,
                                const ThreatMetadata& md);

  // SafeBrowsingDatabaseManager passes its |ui_task_runner| construction
  // parameter to its RefCountedDeleteOnSequence base class, which exposes its
  // passed-in task runner as owning_task_runner(). Expose that |ui_task_runner|
  // parameter internally as ui_task_runner() for clarity.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner() {
    return owning_task_runner();
  }

  typedef std::set<raw_ptr<SafeBrowsingApiCheck, SetExperimental>> ApiCheckSet;

  // In-progress checks. This set owns the SafeBrowsingApiCheck pointers and is
  // responsible for deleting them when removing from the set.
  ApiCheckSet api_checks_;

  // Make callbacks about the completion of database update process. This is
  // currently used by the extension blocklist checker to disable any installed
  // extensions that have been blocklisted since.
  void NotifyDatabaseUpdateFinished();

  // Created and destroyed via StartOnUIThread/StopOnUIThread.
  std::unique_ptr<V4GetHashProtocolManager> v4_get_hash_protocol_manager_;

  // A list of parties to be notified about database updates.
  base::RepeatingClosureList update_complete_callback_list_;

 private:
  // Returns an iterator to the pending API check with the given |client|.
  ApiCheckSet::iterator FindClientApiCheck(Client* client);

};  // class SafeBrowsingDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_DATABASE_MANAGER_H_
