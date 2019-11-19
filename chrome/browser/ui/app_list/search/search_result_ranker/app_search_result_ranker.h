// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_SEARCH_RESULT_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_SEARCH_RESULT_RANKER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

namespace app_list {

class AppLaunchPredictor;

// AppSearchResultRanker is the main class used to train and re-rank the app
// launches.
class AppSearchResultRanker {
 public:
  // Construct a AppSearchResultRanker with profile. It (possibly)
  // asynchronously loads model from disk from |profile_path|; and sets
  // |load_from_disk_success_| to true when the loading finishes.
  // The internal |predictor_| is constructed with param
  // SearchResultRankerPredictorName() in "app_list_features.h".
  // Ephemeral users are speically handled since their profiles are cleaned up
  // after logging out.
  AppSearchResultRanker(const base::FilePath& profile_path,
                        bool is_ephemeral_user);

  ~AppSearchResultRanker();

  // Trains on the |app_id| and (possibly) updates its internal representation.
  void Train(const std::string& app_id);
  // Returns a map of app_id and score.
  //  (1) Higher score means more relevant.
  //  (2) Only returns a subset of app_ids seen by this predictor.
  //  (3) The returned scores should be in range [0.0, 1.0] for
  //      AppSearchProvider to handle.
  base::flat_map<std::string, float> Rank();

 private:
  FRIEND_TEST_ALL_PREFIXES(AppSearchResultRankerSerializationTest,
                           LoadFromDiskSucceed);
  FRIEND_TEST_ALL_PREFIXES(AppSearchResultRankerSerializationTest,
                           LoadFromDiskFailIfNoFileExists);
  FRIEND_TEST_ALL_PREFIXES(AppSearchResultRankerSerializationTest,
                           LoadFromDiskFailWithInvalidProto);
  FRIEND_TEST_ALL_PREFIXES(AppSearchResultRankerSerializationTest,
                           SaveToDiskSucceed);

  // Sets |predictor_| and |load_from_disk_completed_| when
  // LoadPredictorFromDiskOnWorkerThread completes.
  void OnLoadFromDiskComplete(std::unique_ptr<AppLaunchPredictor> predictor);

  // Internal predictor used for train and rank.
  std::unique_ptr<AppLaunchPredictor> predictor_;
  bool load_from_disk_completed_ = false;
  const base::FilePath predictor_filename_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AppSearchResultRanker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppSearchResultRanker);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_SEARCH_RESULT_RANKER_H_
