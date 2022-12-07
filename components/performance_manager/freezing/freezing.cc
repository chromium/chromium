// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/freezing/freezing.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/id_type.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

namespace freezing {

namespace {

// Wrapper for `FreezingVoteToken*` that provides type checking and avoids being
// dereferenced.
class FreezingVoteTokenID
    : public base::IdType<class FreezingVoteTokenTag, std::uintptr_t, 0> {
 public:
  explicit FreezingVoteTokenID(const FreezingVoteToken* token)
      : FreezingVoteTokenID(reinterpret_cast<uintptr_t>(token)) {}

 private:
  constexpr explicit FreezingVoteTokenID(uintptr_t value)
      : base::IdType<FreezingVoteTokenTag, std::uintptr_t, 0>(value) {}

  constexpr static FreezingVoteTokenID FromUnsafeValue(uintptr_t value) {
    return FreezingVoteTokenID(value);
  }
};

// NodeAttachedData used to store the set of FreezingVoteTokenImpl objects
// associated with a PageNode.
class FreezingVoteNodeData : public NodeAttachedDataImpl<FreezingVoteNodeData> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};

  ~FreezingVoteNodeData() override = default;

  void AddVote(FreezingVoteTokenID token);
  void RemoveVote(FreezingVoteTokenID token);
  bool IsEmpty() { return vote_tokens_.empty(); }
  const base::flat_set<FreezingVoteTokenID>& vote_tokens() {
    return vote_tokens_;
  }

 private:
  friend class ::performance_manager::NodeAttachedDataImpl<
      FreezingVoteNodeData>;
  explicit FreezingVoteNodeData(const PageNodeImpl* page_node) {}

  // The freezing votes associated with this node.
  base::flat_set<FreezingVoteTokenID> vote_tokens_;
};

// A registry of FreezingVoteToken that lives on the PM sequence.
//
// There can be multiple freezing votes associated with the same page node.
class FreezingVoteTokenPMRegistry
    : public PageNode::ObserverDefaultImpl,
      public GraphOwned,
      public GraphRegisteredImpl<FreezingVoteTokenPMRegistry> {
 public:
  // A map that associates a voting token to a
  // <FreezingVotingChannel, const PageNode*> pair.
  using VotingChannelsMap =
      base::flat_map<FreezingVoteTokenID,
                     std::pair<FreezingVotingChannel, const PageNode*>>;

  // Returns the FreezingVoteTokenPMRegistry graph owned instance, creates it if
  // necessary. Can only be called from the PM sequence.
  static FreezingVoteTokenPMRegistry* GetOrCreateInstance(Graph* graph);

  FreezingVoteTokenPMRegistry(const FreezingVoteTokenPMRegistry& other) =
      delete;
  FreezingVoteTokenPMRegistry& operator=(const FreezingVoteTokenPMRegistry&) =
      delete;

  // Register a freezing vote for |contents|. |token| is an ID to associate with
  // this vote, there can be only one vote associated with this ID and it as to
  // be passed |UnregisterVote| when the vote is invalidated. This can only be
  // called from the UI thread.
  static void RegisterVoteForWebContents(content::WebContents* contents,
                                         FreezingVoteValue vote_value,
                                         const char* vote_reason,
                                         FreezingVoteTokenID token);

  // Unregister the vote associated with |token|. This can only be called from
  // the UI thread.
  static void UnregisterVote(FreezingVoteTokenID token);

  const VotingChannelsMap& voting_channels_for_testing() {
    return voting_channels_;
  }

 private:
  FreezingVoteTokenPMRegistry() = default;

  // Register a vote for |page_node| on the PM sequence. |token| is an ID
  // associated with this vote that will be used when invalidating it.
  void RegisterVoteOnPMSequence(base::WeakPtr<PageNode> page_node,
                                FreezingVote vote,
                                FreezingVoteTokenID token);

  // Unregister the vote associated with |token|.
  void UnregisterVoteOnPMSequence(FreezingVoteTokenID token);

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Reset the voting channel associated with |voting_channel_iter| and remove
  // this entry from |voting_channels_|. |voting_channel_iter| has to be a valid
  // iterator from |voting_channels_|
  void ResetAndRemoveVotingChannel(
      VotingChannelsMap::iterator& voting_channel_iter,
      const PageNode* page_node);

  VotingChannelsMap voting_channels_ GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// Concrete implementation of a FreezingVoteToken.
class FreezingVoteTokenImpl : public FreezingVoteToken {
 public:
  FreezingVoteTokenImpl(content::WebContents* contents,
                        FreezingVoteValue vote_value,
                        const char* vote_reason);
  ~FreezingVoteTokenImpl() override;
  FreezingVoteTokenImpl(const FreezingVoteTokenImpl& other) = delete;
  FreezingVoteTokenImpl& operator=(const FreezingVoteTokenImpl&) = delete;
};

}  // namespace

FreezingVoteToken::FreezingVoteToken() = default;
FreezingVoteToken::~FreezingVoteToken() = default;

void FreezingVoteNodeData::AddVote(FreezingVoteTokenID token) {
  DCHECK(!base::Contains(vote_tokens_, token));
  vote_tokens_.insert(token);
}

void FreezingVoteNodeData::RemoveVote(FreezingVoteTokenID token) {
  DCHECK(base::Contains(vote_tokens_, token));
  vote_tokens_.erase(token);
}

// static
FreezingVoteTokenPMRegistry* FreezingVoteTokenPMRegistry::GetOrCreateInstance(
    Graph* graph) {
  DCHECK(PerformanceManager::GetTaskRunner()->RunsTasksInCurrentSequence());
  auto* instance = graph->GetRegisteredObjectAs<FreezingVoteTokenPMRegistry>();
  if (!instance) {
    auto registry = base::WrapUnique(new FreezingVoteTokenPMRegistry());
    instance = registry.get();
    graph->PassToGraph(std::move(registry));
  }
  return instance;
}

// static
void FreezingVoteTokenPMRegistry::RegisterVoteForWebContents(
    content::WebContents* contents,
    FreezingVoteValue vote_value,
    const char* vote_reason,
    FreezingVoteTokenID token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Register the vote on the PM sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, FreezingVote vote,
             FreezingVoteTokenID token, Graph* graph) {
            auto* registry =
                FreezingVoteTokenPMRegistry::GetOrCreateInstance(graph);
            registry->RegisterVoteOnPMSequence(page_node, vote, token);
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          FreezingVote(vote_value, vote_reason), token));
}

// static
void FreezingVoteTokenPMRegistry::UnregisterVote(FreezingVoteTokenID token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Unregister the vote on the PM sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](FreezingVoteTokenID token, Graph* graph) {
            auto* registry =
                FreezingVoteTokenPMRegistry::GetOrCreateInstance(graph);
            registry->UnregisterVoteOnPMSequence(token);
          },
          token));
}

void FreezingVoteTokenPMRegistry::RegisterVoteOnPMSequence(
    base::WeakPtr<PageNode> page_node,
    FreezingVote vote,
    FreezingVoteTokenID token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node);

  DCHECK(!base::Contains(voting_channels_, token));

  auto* node_data = FreezingVoteNodeData::GetOrCreate(
      PageNodeImpl::FromNode(page_node.get()));

  auto voting_channel = graph_->GetRegisteredObjectAs<FreezingVoteAggregator>()
                            ->GetVotingChannel();
  voting_channel.SubmitVote(page_node.get(), vote);
  voting_channels_[token] =
      std::make_pair(std::move(voting_channel), page_node.get());

  node_data->AddVote(token);
}

void FreezingVoteTokenPMRegistry::UnregisterVoteOnPMSequence(
    FreezingVoteTokenID token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto voting_channel = voting_channels_.find(token);
  // The vote might be missing from |voting_channels_| if this gets called
  // after the corresponding PageNode has been destroyed.
  if (voting_channel == voting_channels_.end()) {
    return;
  }

  auto* page_node = voting_channel->second.second;
  ResetAndRemoveVotingChannel(voting_channel, page_node);

  auto* node_data =
      FreezingVoteNodeData::Get(PageNodeImpl::FromNode(page_node));
  DCHECK(node_data);
  node_data->RemoveVote(token);

  // Removes the node attached data if there's no more vote associated with this
  // node.
  if (node_data->IsEmpty())
    FreezingVoteNodeData::Destroy(PageNodeImpl::FromNode(page_node));
}

void FreezingVoteTokenPMRegistry::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* node_data =
      FreezingVoteNodeData::Get(PageNodeImpl::FromNode(page_node));

  if (!node_data)
    return;

  // Invalidate the votes if its associated page node is destroyed. This can
  // happen if a freezing vote token is released after the destruction of the
  // WebContents it's associated with.
  for (const auto token_iter : node_data->vote_tokens()) {
    auto voting_channel = voting_channels_.find(token_iter);
    DCHECK(voting_channel != voting_channels_.end());
    ResetAndRemoveVotingChannel(voting_channel, page_node);
  }
}

void FreezingVoteTokenPMRegistry::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RegisterObject(this);
  graph->AddPageNodeObserver(this);
  graph_ = graph;
}

void FreezingVoteTokenPMRegistry::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->UnregisterObject(this);
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void FreezingVoteTokenPMRegistry::ResetAndRemoveVotingChannel(
    VotingChannelsMap::iterator& voting_channel_iter,
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  voting_channel_iter->second.first.InvalidateVote(page_node);
  voting_channel_iter->second.first.Reset();
  voting_channels_.erase(voting_channel_iter);
}

FreezingVoteTokenImpl::FreezingVoteTokenImpl(content::WebContents* contents,
                                             FreezingVoteValue vote_value,
                                             const char* vote_reason) {
  FreezingVoteTokenPMRegistry::RegisterVoteForWebContents(
      contents, vote_value, vote_reason, FreezingVoteTokenID(this));
}

FreezingVoteTokenImpl::~FreezingVoteTokenImpl() {
  FreezingVoteTokenPMRegistry::UnregisterVote(FreezingVoteTokenID(this));
}

std::unique_ptr<FreezingVoteToken> EmitFreezingVoteForWebContents(
    content::WebContents* contents,
    FreezingVoteValue vote_value,
    const char* vote_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::make_unique<FreezingVoteTokenImpl>(contents, vote_value,
                                                 vote_reason);
}

const char* FreezingVoteValueToString(FreezingVoteValue freezing_vote_value) {
  if (freezing_vote_value == freezing::FreezingVoteValue::kCanFreeze) {
    return "kCanFreeze";
  } else {
    return "kCannotFreeze";
  }
}

size_t FreezingVoteCountForPageOnPMForTesting(PageNode* page_node) {
  DCHECK(PerformanceManager::GetTaskRunner()->RunsTasksInCurrentSequence());

  auto* node_data =
      FreezingVoteNodeData::Get(PageNodeImpl::FromNode(page_node));

  if (!node_data)
    return 0;

  return node_data->vote_tokens().size();
}

size_t TotalFreezingVoteCountOnPMForTesting(Graph* graph) {
  DCHECK(PerformanceManager::GetTaskRunner()->RunsTasksInCurrentSequence());

  auto* registry = FreezingVoteTokenPMRegistry::GetOrCreateInstance(graph);

  size_t registry_size = registry->voting_channels_for_testing().size();
  size_t page_nodes_vote_count = 0U;

  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    auto* node_data =
        FreezingVoteNodeData::Get(PageNodeImpl::FromNode(page_node));
    if (node_data)
      page_nodes_vote_count += node_data->vote_tokens().size();
  }

  DCHECK_EQ(registry_size, page_nodes_vote_count);

  return registry_size;
}

}  // namespace freezing
}  // namespace performance_manager
