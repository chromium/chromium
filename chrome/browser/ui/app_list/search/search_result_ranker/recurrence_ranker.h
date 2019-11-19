// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

class FrecencyStore;
class RecurrencePredictor;
class RecurrenceRankerProto;

// |RecurrenceRanker| is the public interface of the ranking system.
class RecurrenceRanker {
 public:
  // |model_identifier| is used for UMA metrics reporting. If it is empty,
  // reporting is disabled. If it is non-empty, an entry should be added to the
  // RecurrenceRankerModel histogram_suffixes entry in the UMA histograms.xml.
  RecurrenceRanker(const std::string& model_identifier,
                   const base::FilePath& filepath,
                   const RecurrenceRankerConfigProto& config,
                   bool is_ephemeral_user);
  ~RecurrenceRanker();

  // Record the use of a given target, and train the predictor on it. This may
  // save to disk, but is not guaranteed to. The user-supplied |condition| can
  // be ignored if it isn't needed.
  void Record(const std::string& target,
              const std::string& condition = std::string());

  // Rename a target, while keeping learned information on it. This may save to
  // disk, but is not guaranteed to.
  void RenameTarget(const std::string& target, const std::string& new_target);
  void RenameCondition(const std::string& condition,
                       const std::string& new_condition);

  // Remove a target or condition entirely. This may save to disk, but is not
  // guaranteed to. If the intention of this removal is to removal all knowledge
  // of, for example, a sensitive target, then a ForceSaveToDisk call should be
  // made after removal.
  void RemoveTarget(const std::string& target);
  void RemoveCondition(const std::string& condition);

  // Returns a map of target to score.
  //  - Higher scores are better.
  //  - Score are guaranteed to be in the range [0,1].
  // The user-supplied |condition| can be ignored if it isn't needed.
  std::map<std::string, float> Rank(
      const std::string& condition = std::string());

  // Returns a sorted vector of <target, score> pairs.
  //  - Higher scores are better.
  //  - Score are guaranteed to be in the range [0,1].
  //  - Pairs are sorted in descending order of score.
  //  - At most n results will be returned.
  // The user-supplied |condition| can be ignored if it isn't needed.
  std::vector<std::pair<std::string, float>> RankTopN(
      int n,
      const std::string& condition = std::string());

  // Force saving all model state to disk. If the user is an ephemeral user,
  // this does nothing. This is not necessary in normal operation, as the ranker
  // automatically saves at regular intervals. Example use: syncing to disk
  // after a target or condition is deleted.
  void SaveToDisk();

  // Return a pointer to the underlying storage of the FrecencyStore for targets
  // or conditions. These should not be used under normal use or ranking.
  // However, they are useful for custom cleanup logic.
  FrecencyStore::ScoreTable* GetTargetData();
  FrecencyStore::ScoreTable* GetConditionData();

  const char* GetPredictorNameForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest,
                           EphemeralUsersUseFrecencyPredictor);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, InitializeIfNoFileExists);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest,
                           SavedRankerRejectedIfConfigMismatched);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, LoadFromDisk);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, SaveToDisk);
  FRIEND_TEST_ALL_PREFIXES(RecurrenceRankerTest, Cleanup);

  // Finishes initialisation by populating |this| with data from the given
  // proto.
  void OnLoadProtoFromDiskComplete(
      std::unique_ptr<RecurrenceRankerProto> proto);

  void MaybeSave();
  void ToProto(RecurrenceRankerProto* proto);
  void FromProto(const RecurrenceRankerProto& proto);

  // Possibly triggers a cleanup of |prdictor_|'s internal state.
  // |proportion_valid| should be the proportion of targets returned by the
  // predictor that exist in the target frecency store. If a cleanup is
  // triggered, RecurrencePredictor::Cleanup is called with a list of valid
  // targets derived from |targets|.
  void MaybeCleanup(float proportion_valid,
                    const FrecencyStore::ScoreTable& targets);

  // Internal predictor that drives ranking.
  std::unique_ptr<RecurrencePredictor> predictor_;

  // Storage for target strings, which maps them to IDs.
  std::unique_ptr<FrecencyStore> targets_;

  // Storage for condition strings, which maps them to IDs.
  std::unique_ptr<FrecencyStore> conditions_;

  // Name used for histogram suffixes.
  const std::string model_identifier_;

  // Where to save the ranker.
  const base::FilePath proto_filepath_;
  // Hash of client-supplied config, used for associating a serialised ranker
  // with a particular config.
  unsigned int config_hash_;

  // Flag indicating the model has finished being loaded and is ready for use.
  bool load_from_disk_completed_ = false;
  // Whether the current user will have their home directory wiped at the end of
  // the session. We adjust the behaviour of the ranker based on this.
  const bool is_ephemeral_user_;

  const base::TimeDelta min_seconds_between_saves_;
  base::Time time_of_last_save_;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<RecurrenceRanker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecurrenceRanker);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_H_
