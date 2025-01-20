// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "components/performance_manager/public/graph/page_node.h"

class GURL;

namespace content {
class WebContents;
}

namespace performance_manager {
class Graph;
}

namespace performance_manager::freezing {

// While alive, this object votes to freeze a `WebContents`. All `WebContents`
// in a browsing instance are frozen if they all have at least one freeze vote
// and no `CannotFreezeReason` applies.
class FreezingVote {
 public:
  explicit FreezingVote(content::WebContents* web_contents);
  ~FreezingVote();

  FreezingVote(const FreezingVote& other) = delete;
  FreezingVote& operator=(const FreezingVote&) = delete;

 private:
  const base::WeakPtr<performance_manager::PageNode> page_node_;
};

// Used to discard frozen pages with growing private memory footprint.
class Discarder {
 public:
  Discarder();
  virtual ~Discarder();

  virtual void DiscardPages(Graph* graph,
                            std::vector<const PageNode*> page_nodes) = 0;
};

// Embedders can implement this to opt out a page from being frozen.
class OptOutChecker {
 public:
  // A callback that should be invoked with a browser context ID string
  // whenever the opt-out policy for that browser context changes.
  using OnPolicyChangedForBrowserContextCallback =
      base::RepeatingCallback<void(std::string_view)>;

  virtual ~OptOutChecker() = default;

  // The freezing policy will call this to set a callback for the embedder to
  // invoke whenever the opt-out policy for a browser context changes.
  virtual void SetOptOutPolicyChangedCallback(
      OnPolicyChangedForBrowserContextCallback callback) = 0;

  // The freezing policy will call this to check if a page with the given
  // `main_frame_url` should be opted out of freezing, according to the freezing
  // policy for `browser_context_id`.
  virtual bool IsPageOptedOutOfFreezing(std::string_view browser_context_id,
                                        const GURL& main_frame_url) = 0;
};

// Returns a list of human-readable reasons why a page can't be frozen
// automatically, or an empty list if it can be frozen automatically. Must be
// invoked on the PM sequence.
std::set<std::string> GetCannotFreezeReasonsForPageNode(
    const PageNode* page_node);

}  // namespace performance_manager::freezing

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
