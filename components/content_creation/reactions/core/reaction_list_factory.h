// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_LIST_FACTORY_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_LIST_FACTORY_H_

#include <vector>

namespace content_creation {

class ReactionMetadata;

// Builds and returns metadata objects for all supported lightweight reactions.
std::vector<ReactionMetadata> BuildReactionMetadata();

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_LIST_FACTORY_H_