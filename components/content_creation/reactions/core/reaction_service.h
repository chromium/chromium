// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_SERVICE_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_SERVICE_H_

#include <vector>

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content_creation {

class ReactionMetadata;

// Keyed service to be used by user-facing surfaces to retrieve the list of
// available lightweight reactions.
class ReactionService : public KeyedService, public base::SupportsUserData {
 public:
  explicit ReactionService();
  ~ReactionService() override;

  // Not copyable or movable.
  ReactionService(const ReactionService&) = delete;
  ReactionService& operator=(const ReactionService&) = delete;

  // Gets the list of available reactions as a vector of |ReactionMetadata|
  // instances.
  std::vector<ReactionMetadata> GetReactions();
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_SERVICE_H_