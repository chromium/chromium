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
  CanonicalTopic(browsing_topics::Topic topic_id, int taxonomy_version);

  // The ID of this topic. A Canonical Topic's ID uniquely identifies it
  // within a specific taxonomy version.
  browsing_topics::Topic topic_id() const { return topic_id_; }

  // The taxonomy version of the Canonical Topic.
  // TODO(crbug.com/40268081): We no longer have a use for the taxonomy
  // version and may want to delete it
  int taxonomy_version() const { return taxonomy_version_; }

  // Returns the localized string representation of the Canonical Topic, this
  // is suitable for direct display to the user.
  std::u16string GetLocalizedRepresentation() const;

  // Returns the localized string description of the Canonical Topic, this
  // is suitable for direct display to the user.
  std::u16string GetLocalizedDescription() const;

  // Functions for converting to and from values for storage in preferences.
  base::Value ToValue() const;
  static std::optional<CanonicalTopic> FromValue(const base::Value& value);

  // TODO(crbug.com/40268081): The less than operator considers
  // `topic_id_` only, because we no longer use the taxonomy version and may
  // want to delete it
  bool operator<(const CanonicalTopic& other) const;

  // TODO(crbug.com/40268081): The equality operator considers
  // `topic_id_` only, because we no longer use the taxonomy version and may
  // want to delete it.
  bool operator==(const CanonicalTopic& other) const;

 private:
  browsing_topics::Topic topic_id_;
  int taxonomy_version_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_CANONICAL_TOPIC_H_
