// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/core/reaction_service.h"

#include "components/content_creation/reactions/core/reaction_list_factory.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"
#include "components/content_creation/reactions/core/reactions_features.h"

namespace content_creation {

ReactionService::ReactionService() = default;

ReactionService::~ReactionService() = default;

std::vector<ReactionMetadata> ReactionService::GetReactions() {
  DCHECK(IsLightweightReactionsEnabled());
  return BuildReactionMetadata();
}

}  // namespace content_creation