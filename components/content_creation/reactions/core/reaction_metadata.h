// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_METADATA_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_METADATA_H_

#include <string>

#include "components/content_creation/reactions/core/reaction_types.h"

namespace content_creation {

// Contains information about a lightweight reaction.
class ReactionMetadata {
 public:
  explicit ReactionMetadata(ReactionType type,
                            const std::string& localized_name,
                            const std::string& thumbnail_url,
                            const std::string& asset_url,
                            int frame_count);

  ReactionMetadata(const ReactionMetadata& other);

  ~ReactionMetadata();

  ReactionType type() const { return type_; }
  const std::string& localized_name() const { return localized_name_; }
  const std::string& thumbnail_url() const { return thumbnail_url_; }
  const std::string& asset_url() const { return asset_url_; }
  int frame_count() const { return frame_count_; }

 private:
  // The reaction type / identifier.
  ReactionType type_;

  // Name of the reaction, used in accessibility strings.
  std::string localized_name_;

  // The URL to the static thumbnail for this reaction.
  std::string thumbnail_url_;

  // The URL to the animated GIF for this reaction.
  std::string asset_url_;

  // The number of animated frames that this reaction has.
  int frame_count_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_METADATA_H_