// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/epoch_topics.h"

#include "base/containers/contains.h"
#include "base/hash/legacy_hash.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/browsing_topics/util.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace browsing_topics {

namespace {

const char kTopTopicsAndObservingDomainsNameKey[] =
    "top_topics_and_observing_domains";
const char kPaddedTopTopicsStartIndexNameKey[] =
    "padded_top_topics_start_index";
const char kConfigVersionNameKey[] = "config_version";
const char kTaxonomyVersionNameKey[] = "taxonomy_version";
const char kModelVersionNameKey[] = "model_version";
const char kCalculationTimeNameKey[] = "calculation_time";
// `taxonomy_size` is a deprecated key. Do not reuse.

bool ShouldUseRandomTopic(uint64_t random_or_top_topic_decision_hash) {
  return base::checked_cast<int>(random_or_top_topic_decision_hash % 100) <
         blink::features::kBrowsingTopicsUseRandomTopicProbabilityPercent.Get();
}

}  // namespace

EpochTopics::EpochTopics(base::Time calculation_time)
    : calculation_time_(calculation_time) {}

EpochTopics::EpochTopics(base::Time calculation_time,
                         CalculatorResultStatus calculator_result_status)
    : calculation_time_(calculation_time),
      calculator_result_status_(calculator_result_status) {
  CHECK_NE(calculator_result_status, CalculatorResultStatus::kSuccess);
}

EpochTopics::EpochTopics(
    std::vector<TopicAndDomains> top_topics_and_observing_domains,
    size_t padded_top_topics_start_index,
    int config_version,
    int taxonomy_version,
    int64_t model_version,
    base::Time calculation_time,
    bool from_manually_triggered_calculation)
    : top_topics_and_observing_domains_(
          std::move(top_topics_and_observing_domains)),
      padded_top_topics_start_index_(padded_top_topics_start_index),
      config_version_(config_version),
      taxonomy_version_(taxonomy_version),
      model_version_(model_version),
      calculation_time_(calculation_time),
      from_manually_triggered_calculation_(from_manually_triggered_calculation),
      calculator_result_status_(CalculatorResultStatus::kSuccess) {
  DCHECK_EQ(base::checked_cast<int>(top_topics_and_observing_domains_.size()),
            blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get());
  DCHECK_LE(padded_top_topics_start_index,
            top_topics_and_observing_domains_.size());
  DCHECK_GT(config_version_, 0);
  DCHECK_GT(taxonomy_version_, 0);
  DCHECK_GT(model_version_, 0);
}

EpochTopics::EpochTopics(EpochTopics&&) = default;

EpochTopics& EpochTopics::operator=(EpochTopics&&) = default;

EpochTopics::~EpochTopics() = default;

// static
EpochTopics EpochTopics::FromDictValue(const base::Value::Dict& dict_value) {
  const base::Value* calculation_time_value =
      dict_value.Find(kCalculationTimeNameKey);
  if (!calculation_time_value)
    return EpochTopics(base::Time());

  base::Time calculation_time =
      base::ValueToTime(calculation_time_value).value();

  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  const base::Value::List* top_topics_and_observing_domains_value =
      dict_value.FindList(kTopTopicsAndObservingDomainsNameKey);
  if (!top_topics_and_observing_domains_value)
    return EpochTopics(calculation_time);

  for (const base::Value& topic_and_observing_domains_value :
       *top_topics_and_observing_domains_value) {
    const base::Value::Dict* topic_and_observing_domains_dict_value =
        topic_and_observing_domains_value.GetIfDict();
    if (!topic_and_observing_domains_dict_value)
      return EpochTopics(calculation_time);

    top_topics_and_observing_domains.push_back(TopicAndDomains::FromDictValue(
        *topic_and_observing_domains_dict_value));
  }

  if (top_topics_and_observing_domains.empty())
    return EpochTopics(calculation_time);

  std::optional<int> padded_top_topics_start_index_value =
      dict_value.FindInt(kPaddedTopTopicsStartIndexNameKey);
  if (!padded_top_topics_start_index_value)
    return EpochTopics(calculation_time);

  size_t padded_top_topics_start_index =
      static_cast<size_t>(*padded_top_topics_start_index_value);

  std::optional<int> config_version_value =
      dict_value.FindInt(kConfigVersionNameKey);

  // `kConfigVersionNameKey` is introduced after the initial release. Instead of
  // treating it as an error and using a fresh epoch, we'll use version 1 which
  // is the only valid version before this field is introduced.
  int config_version = config_version_value ? *config_version_value : 1;

  std::optional<int> taxonomy_version_value =
      dict_value.FindInt(kTaxonomyVersionNameKey);
  if (!taxonomy_version_value)
    return EpochTopics(calculation_time);

  int taxonomy_version = *taxonomy_version_value;

  const base::Value* model_version_value =
      dict_value.Find(kModelVersionNameKey);
  if (!model_version_value)
    return EpochTopics(calculation_time);

  std::optional<int64_t> model_version_int64_value =
      base::ValueToInt64(model_version_value);
  if (!model_version_int64_value)
    return EpochTopics(calculation_time);

  int64_t model_version = *model_version_int64_value;

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     padded_top_topics_start_index, config_version,
                     taxonomy_version, model_version, calculation_time,
                     /*from_manually_triggered_calculation=*/false);
}

base::Value::Dict EpochTopics::ToDictValue() const {
  base::Value::List top_topics_and_observing_domains_list;
  for (const TopicAndDomains& topic_and_domains :
       top_topics_and_observing_domains_) {
    top_topics_and_observing_domains_list.Append(
        topic_and_domains.ToDictValue());
  }

  base::Value::Dict result_dict;
  result_dict.Set(kTopTopicsAndObservingDomainsNameKey,
                  std::move(top_topics_and_observing_domains_list));
  result_dict.Set(kPaddedTopTopicsStartIndexNameKey,
                  base::checked_cast<int>(padded_top_topics_start_index_));
  result_dict.Set(kConfigVersionNameKey, config_version_);
  result_dict.Set(kTaxonomyVersionNameKey, taxonomy_version_);
  result_dict.Set(kModelVersionNameKey, base::Int64ToValue(model_version_));
  result_dict.Set(kCalculationTimeNameKey,
                  base::TimeToValue(calculation_time_));
  return result_dict;
}

CandidateTopic EpochTopics::CandidateTopicForSite(
    const std::string& top_domain,
    const HashedDomain& hashed_context_domain,
    ReadOnlyHmacKey hmac_key) const {
  // The topics calculation failed, or the topics has been cleared.
  if (empty())
    return CandidateTopic::CreateInvalid();

  uint64_t random_or_top_topic_decision_hash =
      HashTopDomainForRandomOrTopTopicDecision(hmac_key, calculation_time_,
                                               top_domain);

  uint64_t top_topic_index_decision_hash =
      HashTopDomainForTopTopicIndexDecision(hmac_key, calculation_time_,
                                            top_domain);

  size_t top_topic_index =
      top_topic_index_decision_hash % top_topics_and_observing_domains_.size();

  const TopicAndDomains& topic_and_observing_domains =
      top_topics_and_observing_domains_[top_topic_index];

  if (!topic_and_observing_domains.IsValid())
    return CandidateTopic::CreateInvalid();

  bool is_true_topic = (top_topic_index < padded_top_topics_start_index_);

  bool should_be_filtered = !topic_and_observing_domains.hashed_domains().count(
      hashed_context_domain);

  if (ShouldUseRandomTopic(random_or_top_topic_decision_hash)) {
    uint64_t random_topic_index_decision =
        HashTopDomainForRandomTopicIndexDecision(hmac_key, calculation_time_,
                                                 top_domain);
    Topic topic = SemanticTree().GetRandomTopic(taxonomy_version(),
                                                random_topic_index_decision);
    return CandidateTopic::Create(topic, /*is_true_topic=*/false,
                                  should_be_filtered, config_version(),
                                  taxonomy_version(), model_version());
  }

  return CandidateTopic::Create(
      topic_and_observing_domains.topic(), is_true_topic, should_be_filtered,
      config_version(), taxonomy_version(), model_version());
}

void EpochTopics::ClearTopics() {
  top_topics_and_observing_domains_.clear();
  padded_top_topics_start_index_ = 0;
}

void EpochTopics::ClearTopic(Topic topic) {
  for (TopicAndDomains& top_topic_and_domains :
       top_topics_and_observing_domains_) {
    // Invalidate `topic_and_domains`. We cannot delete the entry from
    // `top_topics_and_observing_domains_` because it would modify the list of
    // topics, and would break the ability to return the same topic for the same
    // site for the epoch .
    if (top_topic_and_domains.topic() == topic) {
      top_topic_and_domains = TopicAndDomains();
      continue;
    }
    SemanticTree semantic_tree;
    std::vector<Topic> top_topic_ancestors =
        semantic_tree.GetAncestorTopics(top_topic_and_domains.topic());
    if (base::Contains(top_topic_ancestors, topic)) {
      top_topic_and_domains = TopicAndDomains();
    }
  }
}

void EpochTopics::ClearContextDomain(
    const HashedDomain& hashed_context_domain) {
  for (TopicAndDomains& topic_and_domains : top_topics_and_observing_domains_) {
    topic_and_domains.ClearDomain(hashed_context_domain);
  }
}

void EpochTopics::ScheduleExpiration(base::OnceClosure on_expiration_callback) {
  CHECK(!expiration_timer_);
  expiration_timer_ = std::make_unique<base::WallClockTimer>();

  expiration_timer_->Start(
      FROM_HERE,
      calculation_time_ +
          blink::features::kBrowsingTopicsEpochRetentionDuration.Get(),
      std::move(on_expiration_callback));
}

}  // namespace browsing_topics
