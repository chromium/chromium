// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_

#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
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

}  // namespace performance_manager::freezing

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_FREEZING_H_
