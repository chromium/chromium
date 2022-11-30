// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/core/reaction_list_factory.h"

#include "base/strings/stringprintf.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"
#include "components/content_creation/reactions/core/reaction_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
  return {
      ReactionMetadata(
          ReactionType::LAUGH_CRY,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_TEARS_OF_JOY),
          MakeThumbnailUrl("laughcry"), MakeReactionUrl("laughcry"), 48),
      ReactionMetadata(
          ReactionType::HEART,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_BEATING_HEART),
          MakeThumbnailUrl("heart"), MakeReactionUrl("heart"), 48),
      ReactionMetadata(
          ReactionType::EMOTIONAL,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_LOUDLY_CRYING),
          MakeThumbnailUrl("emotional"), MakeReactionUrl("emotional"), 48),
      ReactionMetadata(
          ReactionType::GRIN,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_GRINNING),
          MakeThumbnailUrl("grin"), MakeReactionUrl("grin"), 48),
      ReactionMetadata(
          ReactionType::THANKS,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_FOLDED_HANDS),
          MakeThumbnailUrl("thanks"), MakeReactionUrl("thanks"), 24),
      ReactionMetadata(
          ReactionType::SURPRISE,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_FLUSHED),
          MakeThumbnailUrl("surprise"), MakeReactionUrl("surprise"), 48),
      ReactionMetadata(
          ReactionType::CLAP,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_CLAPPING),
          MakeThumbnailUrl("clap"), MakeReactionUrl("clap"), 24),
      ReactionMetadata(
          ReactionType::UNSURE,
          l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_THINKING),
          MakeThumbnailUrl("unsure"), MakeReactionUrl("unsure"), 48),
      ReactionMetadata(ReactionType::FIRE,
                       l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_FIRE),
                       MakeThumbnailUrl("fire"), MakeReactionUrl("fire"), 48),
      ReactionMetadata(ReactionType::EYES,
                       l10n_util::GetStringUTF8(IDS_LIGHTWEIGHT_REACTIONS_EYES),
                       MakeThumbnailUrl("eyes"), MakeReactionUrl("eyes"), 48),
  };
}

}  // namespace content_creation
