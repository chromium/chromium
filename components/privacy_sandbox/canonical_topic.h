// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_CANONICAL_TOPIC_H_
#define COMPONENTS_PRIVACY_SANDBOX_CANONICAL_TOPIC_H_

#include <string>

#include "base/types/strong_alias.h"
#include "base/values.h"
#include "components/browsing_topics/common/common_types.h"

namespace privacy_sandbox {
// Contains a topic and a name in the current locale.
class CanonicalTopic {
 public:
  // TODO(crbug.com/1286276): Correctly retrieve the set of available
  // taxononmies.
  static const int AVAILABLE_TAXONOMY = 1;

  CanonicalTopic(browsing_topics::Topic topic_id, int taxonomy_version);

  // The ID of this topic. A Canonical Topic's ID uniquely identifies it
  // within a specific taxonomy version.
  browsing_topics::Topic topic_id() const { return topic_id_; }

  // The taxonomy version of the Canonical Topic. Topics with the same topic
  // ID, but different taxonomy versions are never equivalent. Even if their
  // localized representations are identical.
  int taxonomy_version() const { return taxonomy_version_; }

  // Returns the localized string representation of the Canonical Topic, this
  // is suitable for direct display to the user.
  std::u16string GetLocalizedRepresentation() const;

  // Functions for converting to and from values for storage in preferences.
  base::Value ToValue() const;
  static absl::optional<CanonicalTopic> FromValue(const base::Value& value);

  bool operator<(const CanonicalTopic& other) const;
  bool operator==(const CanonicalTopic& other) const;

 private:
  browsing_topics::Topic topic_id_;
  int taxonomy_version_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_CANONICAL_TOPIC_H_
