// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_state.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/util.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

// How often the file is saved at most.
const base::TimeDelta kSaveDelay = base::Milliseconds(2500);

const char kEpochsNameKey[] = "epochs";
const char kNextScheduledCalculationTimeNameKey[] =
    "next_scheduled_calculation_time";
const char kHexEncodedHmacKeyNameKey[] = "hex_encoded_hmac_key";

// `config_version` is a deprecated key. Do not reuse.

std::unique_ptr<BrowsingTopicsState::LoadResult> LoadFileOnBackendTaskRunner(
    const base::FilePath& file_path) {
  bool file_exists = base::PathExists(file_path);

  if (!file_exists) {
    return std::make_unique<BrowsingTopicsState::LoadResult>(
        /*file_exists=*/false, nullptr);
  }

  JSONFileValueDeserializer deserializer(file_path);
  std::unique_ptr<base::Value> value = deserializer.Deserialize(
      /*error_code=*/nullptr,
      /*error_message=*/nullptr);

  return std::make_unique<BrowsingTopicsState::LoadResult>(/*file_exists=*/true,
                                                           std::move(value));
}

bool AreConfigVersionsCompatible(int preexisting, int current) {
  // The config version can be 0 for a failed topics calculation.
  CHECK_GE(preexisting, 0);
  CHECK_GE(current, 1);
  CHECK_LE(current, ConfigVersion::kMaxValue);

  // This could happen in rare case when Chrome rolls back to an earlier
  // version.
  if (preexisting > ConfigVersion::kMaxValue) {
    return false;
  }

  // Epoch from a failed calculation is compatible with any version.
  if (preexisting == 0) {
    return true;
  }

  if (preexisting == current) {
    return true;
  }

  if ((preexisting == ConfigVersion::kInitial &&
       current == ConfigVersion::kUsePrioritizedTopicsList) ||
      (preexisting == ConfigVersion::kUsePrioritizedTopicsList &&
       current == ConfigVersion::kInitial)) {
    // Versions 1 and 2 are forward and backward compatible.
    return true;
  }
  return false;
}

}  // namespace

BrowsingTopicsState::LoadResult::LoadResult(bool file_exists,
                                            std::unique_ptr<base::Value> value)
    : file_exists(file_exists), value(std::move(value)) {}

BrowsingTopicsState::LoadResult::~LoadResult() = default;

BrowsingTopicsState::BrowsingTopicsState(const base::FilePath& profile_path,
                                         base::OnceClosure loaded_callback)
    : backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(profile_path.Append(FILE_PATH_LITERAL("BrowsingTopicsState")),
              backend_task_runner_,
              kSaveDelay,
              /*histogram_suffix=*/"BrowsingTopicsState") {
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadFileOnBackendTaskRunner, writer_.path()),
      base::BindOnce(&BrowsingTopicsState::DidLoadFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(loaded_callback)));
}

BrowsingTopicsState::~BrowsingTopicsState() {
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

void BrowsingTopicsState::ClearAllTopics() {
  DCHECK(loaded_);

  if (!epochs_.empty()) {
    epochs_.clear();
    ScheduleSave();
  }
}

void BrowsingTopicsState::ClearOneEpoch(size_t epoch_index) {
  DCHECK(loaded_);

  epochs_[epoch_index].ClearTopics();
  ScheduleSave();
}

void BrowsingTopicsState::ClearTopic(Topic topic) {
  for (EpochTopics& epoch : epochs_) {
    epoch.ClearTopic(topic);
  }

  ScheduleSave();
}

void BrowsingTopicsState::ClearContextDomain(
    const HashedDomain& hashed_context_domain) {
  for (EpochTopics& epoch : epochs_) {
    epoch.ClearContextDomain(hashed_context_domain);
  }

  ScheduleSave();
}

std::optional<EpochTopics> BrowsingTopicsState::AddEpoch(
    EpochTopics epoch_topics) {
  DCHECK(loaded_);

  epochs_.push_back(std::move(epoch_topics));
  epochs_.back().ScheduleExpiration(base::BindOnce(
      &BrowsingTopicsState::OnEpochExpired, weak_ptr_factory_.GetWeakPtr(),
      epochs_.back().calculation_time()));

  // Remove the epoch data that is no longer useful.
  std::optional<EpochTopics> removed_epoch_topics;
  if (epochs_.size() >
      static_cast<size_t>(
          blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get()) +
          1) {
    removed_epoch_topics = std::move(epochs_[0]);
    epochs_.pop_front();
  }

  ScheduleSave();
  return removed_epoch_topics;
}

void BrowsingTopicsState::ScheduleEpochsExpiration() {
  base::Time expired_calculation_time =
      base::Time::Now() -
      blink::features::kBrowsingTopicsEpochRetentionDuration.Get();

  // Remove expired epochs synchronously.
  base::EraseIf(epochs_, [&expired_calculation_time](const EpochTopics& epoch) {
    return epoch.calculation_time() <= expired_calculation_time;
  });

  for (EpochTopics& epoch : epochs_) {
    epoch.ScheduleExpiration(base::BindOnce(
        &BrowsingTopicsState::OnEpochExpired, weak_ptr_factory_.GetWeakPtr(),
        epoch.calculation_time()));
  }

  ScheduleSave();
}

void BrowsingTopicsState::OnEpochExpired(base::Time calculation_time) {
  // Remove all epochs associated with the given calculation_time.
  // Though calculation times are typically unique, this handles potential
  // duplicates.
  base::EraseIf(epochs_, [&calculation_time](const EpochTopics& epoch) {
    return epoch.calculation_time() == calculation_time;
  });

  ScheduleSave();
}

void BrowsingTopicsState::UpdateNextScheduledCalculationTime(
    base::TimeDelta delay) {
  DCHECK(loaded_);
  DCHECK(!delay.is_negative());

  next_scheduled_calculation_time_ = base::Time::Now() + delay;

  ScheduleSave();
}

std::vector<const EpochTopics*> BrowsingTopicsState::EpochsForSite(
    const std::string& top_domain) const {
  DCHECK(loaded_);

  if (epochs_.empty()) {
    return {};
  }

  const size_t kNumberOfEpochsToExpose = static_cast<size_t>(
      blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get());

  DCHECK_GT(kNumberOfEpochsToExpose, 0u);

  base::Time now = base::Time::Now();

  // Derive a per-user per-site per-epoch time delta in the range of
  // [0, `kBrowsingTopicsMaxEpochIntroductionDelay`). The latest epoch will only
  // be used after `site_epoch_sticky_introduction_delay` has elapsed since the
  // last calculation finish time (i.e. `next_scheduled_calculation_time_` -
  // `kBrowsingTopicsTimePeriodPerEpoch`). This way, each site will see a
  // different epoch switch time.
  base::TimeDelta site_epoch_sticky_introduction_delay =
      CalculateSiteStickyIntroductionDelay(top_domain);

  size_t end_epoch_index = 0;
  if (now <= next_scheduled_calculation_time_ -
                 blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get() +
                 site_epoch_sticky_introduction_delay) {
    if (epochs_.size() < 2) {
      return {};
    }

    end_epoch_index = epochs_.size() - 2;
  } else {
    end_epoch_index = epochs_.size() - 1;
  }

  size_t start_epoch_index = (end_epoch_index + 1 >= kNumberOfEpochsToExpose)
                                 ? end_epoch_index + 1 - kNumberOfEpochsToExpose
                                 : 0;

  std::vector<const EpochTopics*> result;

  for (size_t i = start_epoch_index; i <= end_epoch_index; ++i) {
    const EpochTopics& epoch = epochs_[i];

    base::Time earliest_valid_epoch_time =
        now + CalculateSiteStickyPhaseOutTimeOffset(top_domain, epoch) -
        blink::features::kBrowsingTopicsEpochRetentionDuration.Get();

    if (epoch.calculation_time() > earliest_valid_epoch_time) {
      result.emplace_back(&epoch);
    }
  }

  return result;
}

bool BrowsingTopicsState::HasScheduledSaveForTesting() const {
  return writer_.HasPendingWrite();
}

base::TimeDelta BrowsingTopicsState::CalculateSiteStickyIntroductionDelay(
    const std::string& top_domain) const {
  CHECK(!epochs_.empty());

  uint64_t epoch_introduction_time_decision_hash =
      HashTopDomainForEpochIntroductionTimeDecision(
          hmac_key_, epochs_.back().calculation_time(), top_domain);

  // The random-over period cannot exceed an epoch. This limitation is due to:
  // 1. We only keep data for the last `kNumberOfEpochsToExpose` + 1 epochs. A
  //    longer random-over period would require us to store more historical
  //    epochs to meet the `kNumberOfEpochsToExpose` configuration.
  // 2. For past, non-latest epochs, we don't store the exact delimitation times
  //    (i.e. calculation finish times). Using the calculation start time as an
  //    approximation is not 100% accurate.
  DCHECK_LE(blink::features::kBrowsingTopicsMaxEpochIntroductionDelay.Get(),
            blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get());

  DCHECK_GT(blink::features::kBrowsingTopicsMaxEpochIntroductionDelay.Get()
                .InSeconds(),
            0);

  // If the latest epoch was manually triggered, make the latest epoch
  // immediately available for testing purposes.
  if (epochs_.back().from_manually_triggered_calculation()) {
    return base::Seconds(0);
  }

  return base::Seconds(
      epoch_introduction_time_decision_hash %
      blink::features::kBrowsingTopicsMaxEpochIntroductionDelay.Get()
          .InSeconds());
}

base::TimeDelta BrowsingTopicsState::CalculateSiteStickyPhaseOutTimeOffset(
    const std::string& top_domain,
    const EpochTopics& epoch) const {
  uint64_t epoch_phase_out_time_decision_hash =
      HashTopDomainForEpochPhaseOutTimeDecision(
          hmac_key_, epoch.calculation_time(), top_domain);

  return base::Seconds(
      epoch_phase_out_time_decision_hash %
      blink::features::kBrowsingTopicsMaxEpochPhaseOutTimeOffset.Get()
          .InSeconds());
}

base::ImportantFileWriter::BackgroundDataProducerCallback
BrowsingTopicsState::GetSerializedDataProducerForBackgroundSequence() {
  DCHECK(loaded_);

  return base::BindOnce(
      [](base::Value value) -> std::optional<std::string> {
        // This runs on the background sequence.
        std::string output;
        if (!base::JSONWriter::WriteWithOptions(
                value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
          return std::nullopt;
        }
        return output;
      },
      base::Value(ToDictValue()));
}

base::Value::Dict BrowsingTopicsState::ToDictValue() const {
  DCHECK(loaded_);

  base::Value::List epochs_list;
  for (const EpochTopics& epoch : epochs_) {
    epochs_list.Append(epoch.ToDictValue());
  }

  base::Value::Dict result_dict;
  result_dict.Set(kEpochsNameKey, std::move(epochs_list));

  result_dict.Set(kNextScheduledCalculationTimeNameKey,
                  base::TimeToValue(next_scheduled_calculation_time_));

  std::string hex_encoded_hmac_key = base::HexEncode(hmac_key_);
  result_dict.Set(kHexEncodedHmacKeyNameKey, base::HexEncode(hmac_key_));

  return result_dict;
}

void BrowsingTopicsState::ScheduleSave() {
  DCHECK(loaded_);
  writer_.ScheduleWriteWithBackgroundDataSerializer(this);
}

void BrowsingTopicsState::DidLoadFile(base::OnceClosure loaded_callback,
                                      std::unique_ptr<LoadResult> load_result) {
  DCHECK(load_result);
  DCHECK(!loaded_);

  bool success = false;
  bool should_save_state_to_file = false;

  if (!load_result->file_exists) {
    // If this is the first time loading, generate a `hmac_key_`, and save it.
    // This ensures we only generate the key once per profile, as data derived
    // from the key may be subsequently stored elsewhere.
    hmac_key_ = GenerateRandomHmacKey();
    success = true;
    should_save_state_to_file = true;
  } else if (!load_result->value) {
    // If a file read error was encountered, or if the JSON deserialization
    // failed in general, empty the file.
    should_save_state_to_file = true;
  } else {
    // JSON deserialization succeeded in general. Parse the value to individual
    // fields.
    ParseResult parse_result = ParseValue(*(load_result->value));

    success = parse_result.success;
    should_save_state_to_file = parse_result.should_save_state_to_file;
  }

  base::UmaHistogramBoolean(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", success);

  loaded_ = true;

  if (should_save_state_to_file) {
    ScheduleSave();
  }

  std::move(loaded_callback).Run();
}

BrowsingTopicsState::ParseResult BrowsingTopicsState::ParseValue(
    const base::Value& value) {
  DCHECK(!loaded_);

  const base::Value::Dict* dict_value = value.GetIfDict();
  if (!dict_value) {
    return ParseResult{.success = false, .should_save_state_to_file = true};
  }

  const std::string* hex_encoded_hmac_key =
      dict_value->FindString(kHexEncodedHmacKeyNameKey);
  if (!hex_encoded_hmac_key) {
    return ParseResult{.success = false, .should_save_state_to_file = true};
  }

  if (!base::HexStringToSpan(*hex_encoded_hmac_key, hmac_key_)) {
    // `HexStringToSpan` may partially fill the `hmac_key_` up until the
    // failure. Reset it to empty.
    hmac_key_.fill(0);
    return ParseResult{.success = false, .should_save_state_to_file = true};
  }

  const base::Value::List* epochs_value = dict_value->FindList(kEpochsNameKey);
  if (!epochs_value) {
    return ParseResult{.success = false, .should_save_state_to_file = true};
  }

  for (const base::Value& epoch_value : *epochs_value) {
    const base::Value::Dict* epoch_dict_value = epoch_value.GetIfDict();
    if (!epoch_dict_value) {
      return ParseResult{.success = false, .should_save_state_to_file = true};
    }

    epochs_.push_back(EpochTopics::FromDictValue(*epoch_dict_value));
  }

  for (const EpochTopics& epoch : epochs_) {
    // If any preexisting epoch's version is incompatible with the current
    // version, start with a fresh `epoch_`.
    if (!AreConfigVersionsCompatible(epoch.config_version(),
                                     CurrentConfigVersion())) {
      epochs_.clear();
      return ParseResult{.success = true, .should_save_state_to_file = true};
    }
  }

  const base::Value* next_scheduled_calculation_time_value =
      dict_value->Find(kNextScheduledCalculationTimeNameKey);
  if (!next_scheduled_calculation_time_value) {
    return ParseResult{.success = false, .should_save_state_to_file = true};
  }

  next_scheduled_calculation_time_ =
      base::ValueToTime(next_scheduled_calculation_time_value).value();

  return ParseResult{.success = true, .should_save_state_to_file = false};
}

}  // namespace browsing_topics
