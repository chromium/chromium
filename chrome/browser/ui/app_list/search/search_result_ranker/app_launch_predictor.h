// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.pb.h"

namespace app_list {

// AppLaunchPredictor is the interface implemented by all predictors. It defines
// two basic public functions Train and Rank for training and inferencing.
class AppLaunchPredictor {
 public:
  virtual ~AppLaunchPredictor() = default;
  // Trains on the |app_id| and (possibly) updates its internal representation.
  virtual void Train(const std::string& app_id) = 0;
  // Returns a map of app_id and score.
  //  (1) Higher score means more relevant.
  //  (2) Only returns a subset of app_ids seen by this predictor.
  //  (3) The returned scores should be in range [0.0, 1.0] for
  //      AppSearchProvider to handle.
  virtual base::flat_map<std::string, float> Rank() = 0;
  // Returns the name of the predictor.
  virtual const char* GetPredictorName() const = 0;
  // Whether the model should be saved on disk at this moment.
  virtual bool ShouldSave() = 0;
  // Converts the predictor to AppLaunchPredictorProto.
  virtual AppLaunchPredictorProto ToProto() const = 0;
  // Initializes the predictor with |proto|.
  virtual bool FromProto(const AppLaunchPredictorProto& proto) = 0;
};

// MrfuAppLaunchPredictor is a simple AppLaunchPredictor that balances MRU (most
// recently used) and MFU (most frequently used). It is adopted from LRFU cpu
// cache algorithm.
class MrfuAppLaunchPredictor : public AppLaunchPredictor {
 public:
  MrfuAppLaunchPredictor();
  ~MrfuAppLaunchPredictor() override;

  // AppLaunchPredictor:
  void Train(const std::string& app_id) override;
  base::flat_map<std::string, float> Rank() override;
  const char* GetPredictorName() const override;
  bool ShouldSave() override;
  AppLaunchPredictorProto ToProto() const override;
  bool FromProto(const AppLaunchPredictorProto& proto) override;

  // Name of the predictor;
  static const char kPredictorName[];

 protected:
  // Records last updates of the Score for an app.
  struct Score {
    int32_t num_of_trains_at_last_update = 0;
    float last_score = 0.0f;
  };

  // Updates the Score to now.
  void UpdateScore(Score* score);
  // Map from app_id to its Score.
  base::flat_map<std::string, Score> scores_;
  // Increment 1 for each Train() call.
  int32_t num_of_trains_ = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(AppLaunchPredictorTest, MrfuAppLaunchPredictor);
  friend class SerializedMrfuAppLaunchPredictorTest;

  // Controls how much the score decays for each Train() call.
  // This decay_coeff_ should be within [0.5f, 1.0f]. Setting it as 0.5f means
  // MRU; setting as 1.0f means MFU;
  // TODO(https://crbug.com/871674):
  // (1) Set a better initial value based on real user data.
  // (2) Dynamically change this coeff instead of setting it as constant.
  static constexpr float decay_coeff_ = 0.8f;

  DISALLOW_COPY_AND_ASSIGN(MrfuAppLaunchPredictor);
};

// SerializedMrfuAppLaunchPredictor is MrfuAppLaunchPredictor with supporting of
// AppLaunchPredictor::ToProto and AppLaunchPredictor::FromProto.
class SerializedMrfuAppLaunchPredictor : public MrfuAppLaunchPredictor {
 public:
  SerializedMrfuAppLaunchPredictor();
  ~SerializedMrfuAppLaunchPredictor() override;

  // AppLaunchPredictor:
  const char* GetPredictorName() const override;
  bool ShouldSave() override;
  AppLaunchPredictorProto ToProto() const override;
  bool FromProto(const AppLaunchPredictorProto& proto) override;

  // Name of the predictor;
  static const char kPredictorName[];

 private:
  // Last time the predictor was saved.
  base::Time last_save_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(SerializedMrfuAppLaunchPredictor);
};

// HourAppLaunchPredictor is a AppLaunchPredictor that uses hour of the day as
// bins, and uses app-launch frequency of in each bin as the Rank score.
// For example, if it's 8:30 am right now, then only app-launches between 8am to
// 9am in the last a few days are mainly considered.
// NOTE 1: bins of nearby hours also contributes to the final score but less
//         significient. For example if current time is 8am, then scores in 6am,
//         7am, 9am, and 10am are also added to the final Rank score with
//         smaller weights.
// NOTE 2: workdays and weekends are put into different bins.
class HourAppLaunchPredictor : public AppLaunchPredictor {
 public:
  HourAppLaunchPredictor();
  ~HourAppLaunchPredictor() override;

  // AppLaunchPredictor:
  void Train(const std::string& app_id) override;
  base::flat_map<std::string, float> Rank() override;
  const char* GetPredictorName() const override;
  bool ShouldSave() override;
  AppLaunchPredictorProto ToProto() const override;
  bool FromProto(const AppLaunchPredictorProto& proto) override;

  // Name of the predictor;
  static const char kPredictorName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, GetTheRightBin);
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, RankFromSingleBin);
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, RankFromMultipleBin);
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, CheckDefaultWeights);
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, SetWeightsFromFlag);
  FRIEND_TEST_ALL_PREFIXES(HourAppLaunchPredictorTest, FromProtoDecay);

  // Returns current bin index of this predictor.
  int GetBin() const;

  // Get weights of adjacent bins from flag which will be set using finch config
  // for exploring possible options.
  static std::vector<float> BinWeightsFromFlagOrDefault();

  // The proto for this predictor.
  AppLaunchPredictorProto proto_;
  // Last time the predictor was saved.
  base::Time last_save_timestamp_;
  // Coefficient that controls the decay of previous record.
  static constexpr float kWeeklyDecayCoeff = 0.8;

  DISALLOW_COPY_AND_ASSIGN(HourAppLaunchPredictor);
};

// Predictor for testing AppSearchResultRanker only.
class FakeAppLaunchPredictor : public AppLaunchPredictor {
 public:
  FakeAppLaunchPredictor() = default;
  ~FakeAppLaunchPredictor() override = default;

  // Manually set |should_save_|;
  void SetShouldSave(bool should_save);

  // AppLaunchPredictor:
  void Train(const std::string& app_id) override;
  base::flat_map<std::string, float> Rank() override;
  const char* GetPredictorName() const override;
  bool ShouldSave() override;
  AppLaunchPredictorProto ToProto() const override;
  bool FromProto(const AppLaunchPredictorProto& proto) override;

  // Name of the predictor;
  static const char kPredictorName[];

 private:
  bool should_save_ = false;
  // The proto for this predictor.
  AppLaunchPredictorProto proto_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppLaunchPredictor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_
