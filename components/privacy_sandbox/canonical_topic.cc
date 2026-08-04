// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/canonical_topic.h"

#include "base/check_op.h"

namespace {

// Keys of the value representation of a CanonicalTopic.
constexpr char kTopicId[] = "topicId";
constexpr char kTaxonomyVersion[] = "taxonomyVersion";

}  // namespace

namespace privacy_sandbox {

CanonicalTopic::CanonicalTopic(browsing_topics::Topic topic_id,
                               int taxonomy_version)
    : topic_id_(topic_id), taxonomy_version_(taxonomy_version) {}

base::Value CanonicalTopic::ToValue() const {
  return base::Value(base::DictValue()
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
