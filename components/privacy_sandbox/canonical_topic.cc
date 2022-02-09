// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/canonical_topic.h"

#include <ostream>
#include "base/notreached.h"

namespace privacy_sandbox {

CanonicalTopic::CanonicalTopic(int topic_id, int taxonomy_version)
    : topic_id_(topic_id), taxonomy_version_(taxonomy_version) {}

std::u16string CanonicalTopic::GetLocalizedRepresentation() const {
  // TODO(crbug.com/1286276): Implementation localized representation.
  if (taxonomy_version_ != TEST_TAXONOMY)
    return u"unknown topic";

  switch (topic_id_) {
    case 1:
      return u"Sample Topic 1";
    case 2:
      return u"Sample Topic 2";
    case 3:
      return u"Sample Topic 3";
    case 4:
      return u"Sample Topic 4";
    default:
      NOTREACHED() << "unkown topic" << topic_id_;
      return u"unkown topic";
  }
}

bool CanonicalTopic::operator<(const CanonicalTopic& other) const {
  if (taxonomy_version_ != other.taxonomy_version_)
    return taxonomy_version_ < other.taxonomy_version_;
  return topic_id_ < other.topic_id_;
}

}  // namespace privacy_sandbox