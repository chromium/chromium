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
    browsing_topics::Topic topic_id,
    int taxonomy_version) {
  // The available Taxonomy versions are included in the Chrome binary, and
  // so can be CHECK'd against here.
  CHECK_EQ(privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY,
           taxonomy_version);

  absl::optional<int> localized_name_message_id =
      browsing_topics::SemanticTree().GetLocalizedNameMessageId(
          topic_id, taxonomy_version);

  // Topic IDs  are provided by a categorization model shipped over the network,
  // which could technically  provide Topic IDs outside the expected range, e.g.
  // due to server issues. The case of an out-of-bounds |topic_id| must thus be
  // gracefully handled. To ensure we are made aware of any issues, UMA metrics
  // are logged in this failure case.
  if (!localized_name_message_id.has_value()) {
    base::UmaHistogramSparse("Settings.PrivacySandbox.InvalidTopicIdLocalized",
                             topic_id.value());
    return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_TOPICS_INVALID_TOPIC);
  }

  return l10n_util::GetStringUTF16(localized_name_message_id.value());
}

}  // namespace

namespace privacy_sandbox {

CanonicalTopic::CanonicalTopic(browsing_topics::Topic topic_id,
                               int taxonomy_version)
    : topic_id_(topic_id), taxonomy_version_(taxonomy_version) {}

std::u16string CanonicalTopic::GetLocalizedRepresentation() const {
  return GetLocalizedRepresentationInternal(topic_id_, taxonomy_version_);
}

base::Value CanonicalTopic::ToValue() const {
  base::Value value(base::Value::Type::DICT);
  value.SetKey(kTopicId, base::Value(topic_id_.value()));
  value.SetKey(kTaxonomyVersion, base::Value(taxonomy_version_));
  return value;
}

/*static*/ absl::optional<CanonicalTopic> CanonicalTopic::FromValue(
    const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  auto topic_id = value.GetDict().FindInt(kTopicId);
  if (!topic_id)
    return absl::nullopt;

  auto taxonomy_version = value.GetDict().FindInt(kTaxonomyVersion);
  if (!taxonomy_version)
    return absl::nullopt;

  return CanonicalTopic(browsing_topics::Topic(*topic_id), *taxonomy_version);
}

bool CanonicalTopic::operator<(const CanonicalTopic& other) const {
  if (taxonomy_version_ != other.taxonomy_version_)
    return taxonomy_version_ < other.taxonomy_version_;
  return topic_id_.value() < other.topic_id_.value();
}

bool CanonicalTopic::operator==(const CanonicalTopic& other) const {
  if (taxonomy_version_ != other.taxonomy_version_)
    return false;
  return topic_id_ == other.topic_id_;
}

}  // namespace privacy_sandbox
