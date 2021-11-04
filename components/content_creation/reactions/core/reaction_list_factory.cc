// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/core/reaction_list_factory.h"

#include "base/strings/stringprintf.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"
#include "components/content_creation/reactions/core/reaction_types.h"

namespace {

const char kReactionUrlFormat[] =
    "https://www.gstatic.com/chrome/content-creation/%s.gif";
const char kThumbnailUrlFormat[] =
    "https://www.gstatic.com/chrome/content-creation/thumbnails/%s.png";

std::string MakeReactionUrl(const std::string& reaction_name) {
  return base::StringPrintf(kReactionUrlFormat, reaction_name.c_str());
}

std::string MakeThumbnailUrl(const std::string& reaction_name) {
  return base::StringPrintf(kThumbnailUrlFormat, reaction_name.c_str());
}

}  // namespace

namespace content_creation {

std::vector<ReactionMetadata> BuildReactionMetadata() {
  // TODO(crbug.com/1252182): Localize names.
  // Note: All reactions will use the "clap" asset and thumbnail until the
  // other assets have been uploaded to the server.
  return {
      ReactionMetadata(ReactionType::CLAP, "Clap", MakeThumbnailUrl("clap"),
                       MakeReactionUrl("clap"), 24),
      ReactionMetadata(ReactionType::GRIN, "Grin", MakeThumbnailUrl("grin"),
                       MakeReactionUrl("grin"), 48),
      ReactionMetadata(ReactionType::FIRE, "Fire", MakeThumbnailUrl("fire"),
                       MakeReactionUrl("fire"), 48),
      ReactionMetadata(ReactionType::EYES, "Eyes", MakeThumbnailUrl("eyes"),
                       MakeReactionUrl("eyes"), 48),
      ReactionMetadata(ReactionType::EMOTIONAL, "Emotional",
                       MakeThumbnailUrl("emotional"),
                       MakeReactionUrl("emotional"), 48),
      ReactionMetadata(ReactionType::SURPRISE, "Surprise",
                       MakeThumbnailUrl("surprise"),
                       MakeReactionUrl("surprise"), 48),
      ReactionMetadata(ReactionType::THANKS, "Thanks",
                       MakeThumbnailUrl("thanks"), MakeReactionUrl("thanks"),
                       24),
      ReactionMetadata(ReactionType::UNSURE, "Unsure",
                       MakeThumbnailUrl("unsure"), MakeReactionUrl("unsure"),
                       48),
  };
}

}  // namespace content_creation
