// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_STATE_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_STATE_H_

#include "base/containers/queue.h"
#include "base/files/important_file_writer.h"
#include "base/gtest_prod_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/epoch_topics.h"

namespace browsing_topics {

// Contains the data needed to calculate the browsing topics when a context
// requests it via document.browsingTopics(). The data is backed by a JSON file:
// when `BrowsingTopicsState` is initialized, the state members will be read
// from the file on a backend thread, and all overwriting methods will schedule
// an update to the file. The `BrowsingTopicsState`'s owner should listen on the
// `loaded_callback` notification. Before the loading finishes, it's disallowed
// to access this `BrowsingTopicsState`.
class BrowsingTopicsState
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  struct LoadResult {
    LoadResult(bool file_exists, std::unique_ptr<base::Value> value);
    ~LoadResult();

    LoadResult(const LoadResult&) = delete;
    LoadResult& operator=(const LoadResult&) = delete;
    LoadResult(LoadResult&&) = delete;
    LoadResult& operator=(LoadResult&&) = delete;

    bool file_exists = false;

    // The deserialized value from the content of the json file.
    std::unique_ptr<base::Value> value;
  };

  // The result of parsing a `LoadResult::value` to the `BrowsingTopicsState`.
  struct ParseResult {
    // Whether the parsing was successful. Parsing can fail due to corrupted
    // data.
    bool success = false;

    // Whether `BrowsingTopicsState` should be saved to the file after parsing.
    // Saving is needed if the config version has been updated, or if an error
    // is encountered (to clean up unneeded data). In the common case where the
    // data is loaded from a pre-existing file, the file save isn't necessary.
    bool should_save_state_to_file = false;
  };

  explicit BrowsingTopicsState(const base::FilePath& profile_path,
                               base::OnceClosure loaded_callback);

  ~BrowsingTopicsState() override;

  BrowsingTopicsState(const BrowsingTopicsState&) = delete;
  BrowsingTopicsState& operator=(const BrowsingTopicsState&) = delete;
  BrowsingTopicsState(BrowsingTopicsState&&) = delete;
  BrowsingTopicsState& operator=(BrowsingTopicsState&&) = delete;

  // Clear `epochs_`.
  void ClearAllTopics();

  // Clear the topics data at `epochs_[epoch_index]`. Note that this doesn't
  // remove the entry from `epochs_`.
  void ClearOneEpoch(size_t epoch_index);

  // Clear the topic and observing domains data for `topic`.
  void ClearTopic(Topic topic);

  // Clear the observing domains data in `epochs_`  that match
  // `hashed_context_domain`.
  void ClearContextDomain(const HashedDomain& hashed_context_domain);

  // Append `epoch_topics` to `epochs_`. This is invoked at the end of each
  // epoch calculation. If an old EpochTopics is removed as a result, return it.
  std::optional<EpochTopics> AddEpoch(EpochTopics epoch_topics);

  // Remove expired epochs synchronously. For unexpired epochs, let each one
  // schedule its own expiration task. Upon expiration of each epoch,
  // `OnEpochExpired` will be called to remove it from `epochs_`.
  void ScheduleEpochsExpiration();

  // Calculates the new scheduled time by adding the provided `delay`
  // to the current time (`base::Time::Now()`), and stores the result to
  // `next_scheduled_calculation_time_`.
  void UpdateNextScheduledCalculationTime(base::TimeDelta delay);

  // Calculate the candidate epochs to derive the topics from on `top_domain`.
  // The caller (i.e. BrowsingTopicsServiceImpl, which also holds `this`) is
  // responsible for ensuring that the `EpochTopic` objects that the pointers
  // refer to remain alive when the caller is accessing them.
  std::vector<const EpochTopics*> EpochsForSite(
      const std::string& top_domain) const;

  const base::circular_deque<EpochTopics>& epochs() const {
    DCHECK(loaded_);
    return epochs_;
  }

  base::Time next_scheduled_calculation_time() const {
    DCHECK(loaded_);
    return next_scheduled_calculation_time_;
  }

  ReadOnlyHmacKey hmac_key() const {
    DCHECK(loaded_);
    return hmac_key_;
  }

  bool HasScheduledSaveForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowsingTopicsStateTest,
                           EpochsForSite_OneEpoch_IntroductionTime);
  FRIEND_TEST_ALL_PREFIXES(BrowsingTopicsStateTest,
                           EpochsForSite_OneEpoch_IntroductionTime2);
  FRIEND_TEST_ALL_PREFIXES(BrowsingTopicsStateTest,
                           EpochsForSite_ThreeEpochs_IntroductionTime);
  FRIEND_TEST_ALL_PREFIXES(BrowsingTopicsStateTest,
                           EpochsForSite_OneEpoch_ManuallyTriggered);
  FRIEND_TEST_ALL_PREFIXES(BrowsingTopicsStateTest, EpochsForSite_PhaseOutTime);

  // Calculate the delay between the calculation of the latest epoch and when a
  // site starts seeing that epoch's topics. The site transitions to the latest
  // epoch at a per-site, per-epoch random time within
  // [calculation time, calculation time + max delay).
  base::TimeDelta CalculateSiteStickyIntroductionDelay(
      const std::string& top_domain) const;

  // Calculate the time offset between when a site stops seeing an epoch's
  // topics and when the epoch is actually deleted. The site transitions away
  // from the epoch at a per-site, per-epoch random time within
  // [deletion time - max offset, deletion time].
  //
  // Note: The actual phase-out time can be influenced by the
  // 'kBrowsingTopicsNumberOfEpochsToExpose' setting. If this setting enforces a
  // more restrictive phase-out, that will take precedence.
  base::TimeDelta CalculateSiteStickyPhaseOutTimeOffset(
      const std::string& top_domain,
      const EpochTopics& epoch) const;

  // ImportantFileWriter::BackgroundDataSerializer implementation.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  base::Value::Dict ToDictValue() const;

  void ScheduleSave();

  void DidLoadFile(base::OnceClosure loaded_callback,
                   std::unique_ptr<LoadResult> load_result);

  void OnEpochExpired(base::Time calculation_time);

  // Parse `value` and populate the state member variables.
  ParseResult ParseValue(const base::Value& value);

  // Sequenced task runner where disk writes will be performed.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Helper to write data safely.
  base::ImportantFileWriter writer_;

  // Contains the browsing topics of the latest epochs, as well as the topics
  // observed by each context domain in each of the epoch. These entries are in
  // time ascending order: a new entry will be appended to `epochs_` on every
  // browsing topics calculation, regardless of whether it succeeded or not. We
  // are only interested in the latest
  // `kBrowsingTopicsNumberOfEpochsToExpose + 1` epochs (i.e. the epoch
  // switching time will be per-user, per-site, with a full epoch range of
  // variance, thus one extra epoch are kept here), so old data will be
  // automatically removed, and the size of the queue won't exceed that limit.
  base::circular_deque<EpochTopics> epochs_;

  // The next time a calculation should occur. This will be updated when a
  // calculation is scheduled at the end of a topics calculation and is always
  // synchronously updated with `epochs_`.
  //
  // next_scheduled_calculation_time_.is_null() indicates this is a new profile
  // or there was an update to the configuration version when this
  // `BrowsingTopicsState` is initialized. In either case, `epochs_` will be
  // empty.
  base::Time next_scheduled_calculation_time_;

  // The key for calculating the per-user hash numbers. See ./util.h for various
  // use cases. This key is generated and synced to storage in the first
  // browsing session. It won't be reset/updated in any case.
  HmacKey hmac_key_{};

  // Whether the state members are loaded from file. Public accessor methods are
  // disallowed (except for `HasScheduledSaveForTesting`) before `loaded_`
  // becomes true.
  bool loaded_ = false;

  base::WeakPtrFactory<BrowsingTopicsState> weak_ptr_factory_{this};
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_STATE_H_
