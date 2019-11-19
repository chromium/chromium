// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_FRECENCY_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_FRECENCY_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"

namespace app_list {

// A |FrecencyStore| is a container for a limited set of values, with the
// ability to add, remove, and rename those values. Each value is associated
// with an ID and a score. A value's score decays if it is not updated, but
// other values are, and will eventually be removed. The size of the store is
// limited by, when necessary, removing values with the lowest score. The ID
// associated with a value is guaranteed to be unique (and persists across
// renames), so can be used elsewhere to refer to the stored value.
class FrecencyStore {
 public:
  FrecencyStore(int value_limit, float decay_coeff);
  ~FrecencyStore();

  // Records all information about a value: its id and score, along with the
  // number of updates that had occurred when the score was last calculated.
  // This is used for further score updates.
  struct ValueData {
    unsigned int id;
    float last_score;
    int32_t last_num_updates;
  };

  using ScoreTable = std::map<std::string, FrecencyStore::ValueData>;

  // Record the use of a value. Returns its ID.
  unsigned int Update(const std::string& value);
  // Change one value to another but retain its original ID and score.
  void Rename(const std::string& value, const std::string& new_value);
  // Remove a value and its associated ID from the store entirely.
  void Remove(const std::string& value);

  // Returns the ID for the given value. If the value is not in the store,
  // return base::nullopt.
  base::Optional<unsigned int> GetId(const std::string& value);
  // Return all stored value data. This ensures all scores have been correctly
  // updated, and none of the scores are below the |min_score_| threshold.
  const ScoreTable& GetAll();

  // Returns the underlying storage data structure. This does not ensure scores
  // are correct, and should not be used for scoring items. However it is
  // useful, for example, for implementing custom cleanup logic.
  ScoreTable* get_mutable_values() { return &values_; }

  void ToProto(FrecencyStoreProto* proto) const;
  void FromProto(const FrecencyStoreProto& proto);

 private:
  // Decay the given value's score according to how many training steps have
  // occurred since last update.
  void DecayScore(ValueData* score);
  // Decay the scores of all values in the store, removing those that are at or
  // below |min_threhsold_|.
  void DecayAllScores();
  // Update all scores and, if necessary, reduce the number of saved values to
  // be within the |value_limit_|.
  void Cleanup();

  // The soft-maximum number of values that can be stored. When the actual
  // number of values exceeds 2*|value_limit_|, |Cleanup| is called and the
  // store size is reduced back to exactly |value_limit_|.
  unsigned int value_limit_;
  // Controls how quickly scores decay, in other words controls the trade-off
  // between frequency and recency. This value should be in [0.5, 1.0], where
  // 0.5 makes this an MRU cache and 1.0 makes this a MFU cache.
  float decay_coeff_;

  // This stores all the data of the frecency store.
  ScoreTable values_;

  // Number of times the store has been updated.
  unsigned int num_updates_ = 0;
  // The next ID available for a value to used. This is guaranteed to be unique.
  unsigned int next_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FrecencyStore);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_FRECENCY_STORE_H_
