// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

using FakePredictorConfig = RecurrencePredictorConfigProto::FakePredictorConfig;
using DefaultPredictorConfig =
    RecurrencePredictorConfigProto::DefaultPredictorConfig;
using ConditionalFrequencyPredictorConfig =
    RecurrencePredictorConfigProto::ConditionalFrequencyPredictorConfig;
using FrecencyPredictorConfig =
    RecurrencePredictorConfigProto::FrecencyPredictorConfig;
using FrequencyPredictorConfig =
    RecurrencePredictorConfigProto::FrequencyPredictorConfig;
using HourBinPredictorConfig =
    RecurrencePredictorConfigProto::HourBinPredictorConfig;
using MarkovPredictorConfig =
    RecurrencePredictorConfigProto::MarkovPredictorConfig;
using ExponentialWeightsEnsembleConfig =
    RecurrencePredictorConfigProto::ExponentialWeightsEnsembleConfig;

// |RecurrencePredictor| is the interface for all predictors used by
// |RecurrenceRanker| to drive rankings. If a predictor has some form of
// serialisation, it should have a corresponding proto in
// |recurrence_predictor.proto|.
class RecurrencePredictor {
 public:
  explicit RecurrencePredictor(const std::string& model_identifier);
  virtual ~RecurrencePredictor() = default;

  // Train the predictor on an occurrence of |target| coinciding with
  // |condition|. The predictor will collect its own contextual information, eg.
  // time of day, as part of training.
  virtual void Train(unsigned int target, unsigned int condition) = 0;

  // Return a map of all known targets to their scores for the given condition
  // under this predictor. Scores must be within the range [0,1].
  virtual std::map<unsigned int, float> Rank(unsigned int condition) = 0;

  // Called when the ranker detects that the predictor's rankings are
  // significantly different to the set of valid targets, and can optionally be
  // used to clean up internal state. For efficiency reasons, Cleanup is
  // supplied a const reference to the FrecencyStore's internal state. However
  // it is likely that only id field in the values is of interest.
  virtual void Cleanup(const std::vector<unsigned int>& valid_targets) {}

  virtual void ToProto(RecurrencePredictorProto* proto) const = 0;
  virtual void FromProto(const RecurrencePredictorProto& proto) = 0;
  virtual const char* GetPredictorName() const = 0;

 protected:
  // The name of the model which this predictor belongs to. Used for metrics
  // reporting.
  std::string model_identifier_;
};

// FakePredictor is a simple 'predictor' used for testing. Rank() returns the
// numbers of times each target has been trained on, and does not handle
// conditions.
//
// WARNING: this breaks the guarantees on the range of values a score can take,
// so should not be used for anything except testing.
class FakePredictor : public RecurrencePredictor {
 public:
  explicit FakePredictor(const std::string& model_identifier);
  FakePredictor(const FakePredictorConfig& config,
                const std::string& model_identifier);
  ~FakePredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void Cleanup(const std::vector<unsigned int>& valid_targets) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  std::map<unsigned int, float> counts_;

  DISALLOW_COPY_AND_ASSIGN(FakePredictor);
};

// DefaultPredictor does no work on its own. Using this predictor makes the
// RecurrenceRanker return the scores of its FrecencyStore instead of using a
// predictor.
class DefaultPredictor : public RecurrencePredictor {
 public:
  DefaultPredictor(const DefaultPredictorConfig& config,
                   const std::string& model_identifier);
  ~DefaultPredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPredictor);
};

// A simple frequency predictor that scores targets by their normalized counts.
class FrequencyPredictor : public RecurrencePredictor {
 public:
  explicit FrequencyPredictor(const std::string& model_identifier);
  FrequencyPredictor(const FrequencyPredictorConfig& config,
                     const std::string& model_identifier);
  ~FrequencyPredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void Cleanup(const std::vector<unsigned int>& valid_targets) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  std::map<unsigned int, int> counts_;

  DISALLOW_COPY_AND_ASSIGN(FrequencyPredictor);
};

// Represents a conditional probability table which stores the frequency of
// targets given a condition. Conditions can be client-provided, or could be the
// output of a hash of other contextual features. This allows for an arbitrary
// number of conditions to be used.
class ConditionalFrequencyPredictor : public RecurrencePredictor {
 public:
  // The predictor doesn't use any configuration values, so a zero-argument
  // constructor is also provided for convenience when this is used within other
  // predictors.
  ConditionalFrequencyPredictor(
      const ConditionalFrequencyPredictorConfig& config,
      const std::string& model_identifier);
  explicit ConditionalFrequencyPredictor(const std::string& model_identifier);
  ~ConditionalFrequencyPredictor() override;

  // Stores a mapping from events to frequencies, along with the total frequency
  // of all events.
  class Events {
   public:
    Events();
    Events(const Events& other);
    ~Events();

    std::map<unsigned int, float> freqs;
    float total = 0.0f;
  };

  // RecurrencePredictor:
  // Add 1.0f to the relative frequency of |target| given |condition|.
  void Train(unsigned int target, unsigned int condition) override;
  // The scores in the returned map sum to 1 if the map is non-empty.
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void Cleanup(const std::vector<unsigned int>& valid_targets) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  // Deletes all information about conditions not in |valid_conditions|. This is
  // analogous to Cleanup for targets. Note that Cleanup already deletes
  // conditions if they have no associated targets, so CleanupTargets is useful
  // only in the case of having extra information about invalid conditions.
  void CleanupConditions(const std::vector<unsigned int>& valid_conditions);

  static const char kPredictorName[];

  // Add |delta| to the relative frequency of |target| given |condition|.
  void TrainWithDelta(unsigned int target, unsigned int condition, float delta);

 private:
  // Stores a mapping from conditions to events to frequencies.
  std::map<unsigned int, ConditionalFrequencyPredictor::Events> table_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalFrequencyPredictor);
};

// FrecencyPredictor ranks targets according to their frecency, and
// can only be used for zero-state predictions. This predictor has two
// key differences from the DefaultPredictor:
//
//  1. The decay coefficient for ranking can be set separately from the
//     RecurrenceRanker's target_decay parameter used for storage.
//
//  2. The scores returned by FrecencyPredictor::Rank are normalized to sum to
//     1 (if there is at least one result). This is not the case for
//     DefaultPredictor::Rank.
//
// If neither of the above differences are required, it is more efficient to
// use DefaultPredictor.
class FrecencyPredictor : public RecurrencePredictor {
 public:
  FrecencyPredictor(const FrecencyPredictorConfig& config,
                    const std::string& model_identifier);
  ~FrecencyPredictor() override;

  // Records all information about a target: its id and score, along with the
  // number of updates that had occurred when the score was last calculated.
  // This is used for further score updates.
  struct TargetData {
    float last_score = 0.0f;
    int32_t last_num_updates = 0;
  };

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void Cleanup(const std::vector<unsigned int>& valid_targets) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  // Decay the given target's score according to how many training steps have
  // occurred since last update.
  void DecayScore(TargetData* score);
  // Decay the scores of all targets.
  void DecayAllScores();

  // Controls how quickly scores decay, in other words controls the trade-off
  // between frequency and recency.
  const float decay_coeff_;

  // Number of times the store has been updated.
  unsigned int num_updates_ = 0;

  // This stores all the data of the frecency predictor.
  std::map<unsigned int, FrecencyPredictor::TargetData> targets_;

  DISALLOW_COPY_AND_ASSIGN(FrecencyPredictor);
};

// |HourBinPredictor| ranks targets according to their frequency during
// the current and neighbor hour bins. It can only be used for zero-state
// predictions.
class HourBinPredictor : public RecurrencePredictor {
 public:
  HourBinPredictor(const HourBinPredictorConfig& config,
                   const std::string& model_identifier);
  ~HourBinPredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(HourBinPredictorTest, GetTheRightBin);
  FRIEND_TEST_ALL_PREFIXES(HourBinPredictorTest, TrainAndRankSingleBin);
  FRIEND_TEST_ALL_PREFIXES(HourBinPredictorTest, TrainAndRankMultipleBin);
  FRIEND_TEST_ALL_PREFIXES(HourBinPredictorTest, ToProto);
  FRIEND_TEST_ALL_PREFIXES(HourBinPredictorTest, FromProtoDecays);

  // Return the bin index that is |hour_difference| away from the current bin
  // index.
  int GetBinFromHourDifference(int hour_difference) const;
  // Return the current bin index of this predictor.
  int GetBin() const;
  // Check decay condition.
  bool ShouldDecay();
  // Decay the frequency of all items.
  void DecayAll();
  void SetLastDecayTimestamp(float value) {
    proto_.set_last_decay_timestamp(value);
  }

  HourBinPredictorProto proto_;

  // Weightings for how much an update in bin should affect the bins around it.
  // Keys in the map are relative indices from the updated bin.
  std::map<int, float> bin_weights_;

  // How much to decay frequencies each week.
  float weekly_decay_coeff_;

  DISALLOW_COPY_AND_ASSIGN(HourBinPredictor);
};

// A first-order Markov chain that predicts the next target from the previous.
// It does not use the condition.
class MarkovPredictor : public RecurrencePredictor {
 public:
  MarkovPredictor(const MarkovPredictorConfig& config,
                  const std::string& model_identifier);
  ~MarkovPredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void Cleanup(const std::vector<unsigned int>& valid_targets) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(MarkovPredictorTest, Cleanup);
  FRIEND_TEST_ALL_PREFIXES(MarkovPredictorTest, ToAndFromProto);

  // Stores transition probabilities: P(target | previous_target).
  std::unique_ptr<ConditionalFrequencyPredictor> frequencies_;

  // The most recently observed target.
  base::Optional<unsigned int> previous_target_;

  DISALLOW_COPY_AND_ASSIGN(MarkovPredictor);
};

// A predictor that uses a weighted ensemble of other predictors' scores. Any
// number of constituent predictors can be configured. On training, all
// constituent predictors are trained, and the ensemble model itself learns
// weights for each predictor. At inference, the ensemble ranks targets based on
// a weighted average of its predictors.
//
// The weights for each predictor are trained with the exponential weights
// algorithm. If |t'| is the correct target The weight w_i for predictor i is
// updated according to:
//   w_i' = w_i * exp(-learning_rate * p_i(t = t')),
// where p_i(t) is probability (normalized score) of target t returned by
// predictor i. Weights are kept normalized to sum to 1.
class ExponentialWeightsEnsemble : public RecurrencePredictor {
 public:
  ExponentialWeightsEnsemble(const ExponentialWeightsEnsembleConfig& config,
                             const std::string& model_identifier);
  ~ExponentialWeightsEnsemble() override;

  // RecurrencePredictor:
  void Train(unsigned int target, unsigned int condition) override;
  std::map<unsigned int, float> Rank(unsigned int condition) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(ExponentialWeightsEnsembleTest,
                           GoodModelAndBadModel);
  FRIEND_TEST_ALL_PREFIXES(ExponentialWeightsEnsembleTest, TwoBalancedModels);

  // Pairs of the constituent predictors, and their weights.
  std::vector<std::pair<std::unique_ptr<RecurrencePredictor>, float>>
      predictors_;

  float learning_rate_ = 0.0f;

  DISALLOW_COPY_AND_ASSIGN(ExponentialWeightsEnsemble);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
