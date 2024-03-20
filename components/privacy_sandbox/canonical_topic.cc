// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/canonical_topic.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Keys of the value representation of a CanonicalTopic.
constexpr char kTopicId[] = "topicId";
constexpr char kTaxonomyVersion[] = "taxonomyVersion";

std::u16string GetLocalizedRepresentationInternal(
    browsing_topics::Topic topic_id) {
  browsing_topics::SemanticTree semantic_tree;

  std::optional<int> localized_name_message_id =
      semantic_tree.GetLatestLocalizedNameMessageId(topic_id);

  // Topic IDs  are provided by a categorization model shipped over the network,
  // which could technically  provide Topic IDs outside the expected range, e.g.
  // due to server issues. The case of an out-of-bounds |topic_id| must thus be
  // gracefully handled. To ensure we are made aware of any issues, UMA metrics
  // are logged in this failure case.
  if (!localized_name_message_id.has_value()) {
    return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_TOPICS_INVALID_TOPIC);
  }

  return l10n_util::GetStringUTF16(localized_name_message_id.value());
}

std::u16string GetLocalizedDescriptionInternal(
    browsing_topics::Topic topic_id) {
  browsing_topics::SemanticTree semantic_tree;

  auto children =
      semantic_tree.GetAtMostTwoRepresentativesInCurrentTaxonomy(topic_id);

  if (children.size() == 0 || children.size() > 2) {
    return std::u16string();
  }

  if (children.size() == 1) {
    return l10n_util::GetStringUTF16(
        semantic_tree.GetLatestLocalizedNameMessageId(children[0]).value());
  }

  std::optional<int> message_id_1 =
      semantic_tree.GetLatestLocalizedNameMessageId(children[0]);
  std::optional<int> message_id_2 =
      semantic_tree.GetLatestLocalizedNameMessageId(children[1]);

  return l10n_util::GetStringFUTF16(
      IDS_SETTINGS_TOPICS_PAGE_FIRST_LEVEL_TOPIC_DESCRIPTOR,
      l10n_util::GetStringUTF16(message_id_1.value()),
      l10n_util::GetStringUTF16(message_id_2.value()));
}

}  // namespace

namespace privacy_sandbox {

CanonicalTopic::CanonicalTopic(browsing_topics::Topic topic_id,
                               int taxonomy_version)
    : topic_id_(topic_id), taxonomy_version_(taxonomy_version) {}

std::u16string CanonicalTopic::GetLocalizedRepresentation() const {
  return GetLocalizedRepresentationInternal(topic_id_);
}

std::u16string CanonicalTopic::GetLocalizedDescription() const {
  return GetLocalizedDescriptionInternal(topic_id_);
}

base::Value CanonicalTopic::ToValue() const {
  return base::Value(base::Value::Dict()
                         .Set(kTopicId, topic_id_.value())
                         .Set(kTaxonomyVersion, taxonomy_version_));
}

/*static*/ std::optional<CanonicalTopic> CanonicalTopic::FromValue(
    const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  auto topic_id = value.GetDict().FindInt(kTopicId);
  if (!topic_id) {
    return std::nullopt;
  }

  auto taxonomy_version = value.GetDict().FindInt(kTaxonomyVersion);
  if (!taxonomy_version) {
    return std::nullopt;
  }

  return CanonicalTopic(browsing_topics::Topic(*topic_id), *taxonomy_version);
}

bool CanonicalTopic::operator<(const CanonicalTopic& other) const {
  return topic_id_.value() < other.topic_id_.value();
}

bool CanonicalTopic::operator==(const CanonicalTopic& other) const {
  return topic_id_ == other.topic_id_;
}

}  // namespace privacy_sandbox
