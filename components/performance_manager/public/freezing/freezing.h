// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header contains some definition for some types used by the page freezing
// logic.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_

#include <memory>

#include "components/performance_manager/public/voting/voting.h"

namespace content {
class WebContents;
}

namespace performance_manager {

class Graph;
class PageNode;

namespace freezing {

enum class FreezingVoteValue {
  kCannotFreeze,
  kCanFreeze,
};

using FreezingVote =
    voting::Vote<PageNode, FreezingVoteValue, FreezingVoteValue::kCannotFreeze>;
using FreezingVoterId = voting::VoterId<FreezingVote>;
using FreezingVoteObserver = voting::VoteObserver<FreezingVote>;
using FreezingVotingChannel = voting::VotingChannel<FreezingVote>;
using FreezingVotingChannelFactory = voting::VotingChannelFactory<FreezingVote>;

// A freezing vote token, instances of this are meant to be retrieved by calling
// |EmitFreezingVoteForWebContents|.
class FreezingVoteToken {
 public:
  FreezingVoteToken(const FreezingVoteToken& other) = delete;
  FreezingVoteToken& operator=(const FreezingVoteToken&) = delete;
  virtual ~FreezingVoteToken() = 0;

 protected:
  FreezingVoteToken();
};

// Allows emitting a freezing vote for a WebContents. The vote's lifetime will
// follow the lifetime of this object, as soon as it's released the vote will be
// invalidated. This can only be called from the UI thread.
//
// NOTE: |vote_reason| *must* be a static string.
std::unique_ptr<FreezingVoteToken> EmitFreezingVoteForWebContents(
    content::WebContents* contents,
    FreezingVoteValue vote_value,
    const char* vote_reason);

// Converts a FreezingVoteValue to a textual representation.
const char* FreezingVoteValueToString(FreezingVoteValue freezing_vote_value);

// Used to retrieve the number of freezing votes associated with |page_node|.
size_t FreezingVoteCountForPageOnPMForTesting(PageNode* page_node);

// Get the total number of freezing votes.
size_t TotalFreezingVoteCountOnPMForTesting(Graph* graph);

}  // namespace freezing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
