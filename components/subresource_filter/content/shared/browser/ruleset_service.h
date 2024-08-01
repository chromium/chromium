// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the top-level class for the RulesetService.  There are
// associated classes that tie this into the dealer as well as the filter
// agents.  The distribution pipeline looks like this:
//
//                      RulesetService
//                           |
//                           v                  Browser
//                 RulesetPublisher(Impl)
//                     |              |
//        - - - - - - -|- - - - - - - |- - - - - - - - - -
//                     |       |      |
//                     v              v
//          *RulesetDealer     |  *RulesetDealer
//                 |                |       |
//                 |           |    |       v
//                 v                |      SubresourceFilterAgent
//    SubresourceFilterAgent   |    v
//                                SubresourceFilterAgent
//                             |
//
//         Renderer #1         |          Renderer #n
//
// Note: UnverifiedRulesetDealer is shortened to *RulesetDealer above. There is
// also a VerifiedRulesetDealer which is used similarly on the browser side.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_SERVICE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version.h"
#include "components/subresource_filter/content/shared/browser/ruleset_publisher.h"
#include "components/subresource_filter/core/browser/ruleset_version.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/ruleset_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace subresource_filter {

class RulesetIndexer;
class UnindexedRulesetStreamGenerator;

// Contains all utility functions that govern how files pertaining to indexed
// ruleset version should be organized on disk.
//
// The various indexed ruleset versions are kept in a two-level directory
// hierarchy based on their format and content version numbers, like so:
//
//   |base_dir|
//    |
//    +--10 (format_version)
//    |  |
//    |  +--1 (content_version)
//    |  |   \...
//    |  |
//    |  +--2 (content_version)
//    |      \...
//    |
//    +--11 (format_version)
//       |
//       +--2 (content_version)
//           \...
//
class IndexedRulesetLocator {
 public:
  // Returns a path to a directory under |base_dir| where files corresponding to
  // the given |version| should be stored.
  static base::FilePath GetSubdirectoryPathForVersion(
      const base::FilePath& base_dir,
      const IndexedRulesetVersion& version);

  static base::FilePath GetRulesetDataFilePath(
      const base::FilePath& version_directory);
  static base::FilePath GetLicenseFilePath(
      const base::FilePath& version_directory);
  static base::FilePath GetSentinelFilePath(
      const base::FilePath& version_directory);

  // Cleans up the |indexed_ruleset_base_dir| by deleting all obsoleted ruleset
  // versions, keeping only:
  //  -- the |most_recent_version|, if it is valid,
  //  -- versions of the current format that have a sentinel file present.
  // To be called on the |background_task_runner_|.
  static void DeleteObsoleteRulesets(
      const base::FilePath& indexed_ruleset_base_dir,
      const IndexedRulesetVersion& most_recent_version);
};

// Responsible for indexing subresource filtering rules that are downloaded
// through the component updater; for versioned storage of the indexed ruleset;
// and for supplying the most up-to-date version of the indexed ruleset to the
// RulesetPublisher, provided in the constructor, that abstracts away
// distribution of the ruleset to renderers.
//
// The service is parametrized for different rulesets via |config|. Files
// corresponding to each version of the indexed ruleset are stored in a separate
// subdirectory inside |indexed_ruleset_base_dir| named after the version. The
// version information of the most recent successfully stored ruleset is written
// into |local_state|. The invariant is maintained that the version pointed to
// by preferences, if valid, will exist on disk at any point in time.
//
// Obsolete files deletion and rulesets indexing are posted to
// |background_task_runner|.
class RulesetService {
 public:
  // Enumerates the possible outcomes of indexing a ruleset and writing it to
  // disk. Used in UMA histograms, so the order of enumerators should not be
  // changed.
  enum class IndexAndWriteRulesetResult {
    SUCCESS,
    FAILED_CREATING_SCRATCH_DIR,
    FAILED_WRITING_RULESET_DATA,
    FAILED_WRITING_LICENSE,
    FAILED_REPLACE_FILE,
    FAILED_DELETE_PREEXISTING,
    FAILED_OPENING_UNINDEXED_RULESET,
    FAILED_PARSING_UNINDEXED_RULESET,
    FAILED_CREATING_VERSION_DIR,
    FAILED_CREATING_SENTINEL_FILE,
    FAILED_DELETING_SENTINEL_FILE,
    ABORTED_BECAUSE_SENTINEL_FILE_PRESENT,

    // Insert new values before this line.
    MAX,
  };

  // Creates a new instance of a ruleset with common configuration for
  // production usage in embedders.
  static std::unique_ptr<RulesetService> Create(
      const RulesetConfig& config,
      PrefService* local_state,
      const base::FilePath& user_data_dir,
      const RulesetPublisher::Factory& publisher_factory);

  // Creates a new instance of a ruleset This is then assigned to a
  // RulesetPublisher that calls Initialize for this ruleset service.  Starts
  // initialization of the RulesetService, performing tasks that won't slow down
  // Chrome startup, then queues the FinishInitialization task.
  // NOTE: This constructor supports specifying various params explicitly for
  // tests. Production code should favor RulesetService::Create().
  RulesetService(
      const RulesetConfig& config,
      PrefService* local_state,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& indexed_ruleset_base_dir,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const RulesetPublisher::Factory& publisher_factory);

  RulesetService(const RulesetService&) = delete;
  RulesetService& operator=(const RulesetService&) = delete;

  virtual ~RulesetService();

  // Pass-through function to set the callback on publishing.
  void SetRulesetPublishedCallbackForTesting(base::OnceClosure callback) {
    publisher_->SetRulesetPublishedCallbackForTesting(std::move(callback));
  }

  // Indexes, stores, and publishes the given unindexed ruleset, unless its
  // |content_version| matches that of the most recently indexed version, in
  // which case it does nothing. The files comprising the unindexed ruleset
  // need to remain accessible even after the method returns.
  //
  // Computation-heavy steps and I/O are performed on a background thread.
  // Furthermore, to prevent start-up congestion, new rulesets provided via this
  // method will not be processed until after start-up.
  //
  // Virtual so that it can be mocked out in tests.
  virtual void IndexAndStoreAndPublishRulesetIfNeeded(
      const UnindexedRulesetInfo& unindexed_ruleset_info);

  // Get the ruleset version associated with the current local_state_.
  IndexedRulesetVersion GetMostRecentlyIndexedVersion() const;

  VerifiedRulesetDealer::Handle* GetRulesetDealer() {
    return publisher_->GetRulesetDealer();
  }

  RulesetConfig config() const { return config_; }

 private:
  friend class SubresourceFilteringRulesetServiceTest;
  friend class SubresourceFilterBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(
      SubresourceFilterRulesetPublisherTest,
      PublishedRuleset_IsDistributedToExistingAndNewRenderers);
  FRIEND_TEST_ALL_PREFIXES(SubresourceFilterRulesetPublisherTest,
                           PublishesRulesetInOnePostTask);
  FRIEND_TEST_ALL_PREFIXES(SubresourceFilteringRulesetServiceTest,
                           NewRuleset_WriteFailure);
  FRIEND_TEST_ALL_PREFIXES(SubresourceFilteringRulesetServiceDeathTest,
                           NewRuleset_IndexingCrash);

  using WriteRulesetCallback =
      base::OnceCallback<void(const IndexedRulesetVersion&)>;

  // Reads the ruleset described in |unindexed_ruleset_info|, indexes it, and
  // calls WriteRuleset() to persist the indexed ruleset. Returns the resulting
  // indexed ruleset version, or an invalid version on error. To be called on
  // the |background_task_runner|.
  static IndexedRulesetVersion IndexAndWriteRuleset(
      const RulesetConfig& config,
      const base::FilePath& indexed_ruleset_base_dir,
      const UnindexedRulesetInfo& unindexed_ruleset_info);

  // Reads the rules via the |unindexed_ruleset_stream_generator|, and indexes
  // them using |indexer|. Returns whether the entire ruleset could be parsed.
  static bool IndexRuleset(
      const RulesetConfig& config,
      UnindexedRulesetStreamGenerator* unindexed_ruleset_stream_generator,
      RulesetIndexer* indexer);

  // Writes all files comprising the given |indexed_version| of the ruleset
  // into the corresponding subdirectory in |indexed_ruleset_base_dir|.
  // More specifically, it writes:
  //  -- the |indexed_ruleset_data|,
  //  -- a copy of the LICENSE file at |license_path|, if exists.
  // Returns true on success. To be called on the |background_task_runner|.
  // Attempts not to leave an incomplete copy in the target directory.
  //
  // Writing is factored out into this separate function so it can be
  // independently exercised in tests.
  static IndexAndWriteRulesetResult WriteRuleset(
      const base::FilePath& indexed_ruleset_version_dir,
      const base::FilePath& license_source_path,
      base::span<const uint8_t> indexed_ruleset_data);

  // Indirections for accessing these routines, so as to allow overriding and
  // injecting faults in tests.
  static decltype(&IndexRuleset) g_index_ruleset_func;
  static decltype(&base::ReplaceFile) g_replace_file_func;

  // Runs as a BEST_EFFORT task to complete portions of the initialization that
  // could potentially block Chrome startup.  Once this task is reached, the
  // RulesetService is considered initialized.
  void FinishInitialization();

  // Posts a task to the |background_task_runner| to index and persist the
  // given unindexed ruleset. Then, on success, updates the most recently
  // indexed version in preferences and invokes |success_callback| on the
  // calling thread. There is no callback on failure.
  void IndexAndStoreRuleset(const UnindexedRulesetInfo& unindexed_ruleset_info,
                            WriteRulesetCallback success_callback);

  void OnWrittenRuleset(WriteRulesetCallback result_callback,
                        const IndexedRulesetVersion& version);

  void OpenAndPublishRuleset(const IndexedRulesetVersion& version);
  void OnRulesetSet(RulesetFilePtr file);

  const RulesetConfig config_;

  const raw_ptr<PrefService> local_state_;

  // Obsolete files deletion and indexing should be done on this runner.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  std::unique_ptr<RulesetPublisher> publisher_;

  UnindexedRulesetInfo queued_unindexed_ruleset_info_;
  bool is_initialized_;

  const base::FilePath indexed_ruleset_base_dir_;

  base::WeakPtrFactory<RulesetService> weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_SERVICE_H_
