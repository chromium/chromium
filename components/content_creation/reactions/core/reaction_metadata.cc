// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/core/reaction_metadata.h"

namespace content_creation {

ReactionMetadata::ReactionMetadata(ReactionType type,
                                   const std::string& localized_name,
                                   const std::string& thumbnail_url,
                                   const std::string& asset_url,
                                   int frame_count)
    : type_(type),
      localized_name_(localized_name),
      thumbnail_url_(thumbnail_url),
      asset_url_(asset_url),
      frame_count_(frame_count) {}

ReactionMetadata::ReactionMetadata(const ReactionMetadata& other)
    : type_(other.type()),
      localized_name_(other.localized_name()),
      thumbnail_url_(other.thumbnail_url()),
      asset_url_(other.asset_url()),
      frame_count_(other.frame_count()) {}

ReactionMetadata::~ReactionMetadata() = default;

}  // namespace content_creation