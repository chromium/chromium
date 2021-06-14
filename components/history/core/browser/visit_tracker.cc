// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_tracker.h"

#include <stddef.h>


namespace history {

// When the list gets longer than 'MaxItems', CleanupTransitionList will resize
// the list down to 'ResizeTo' size. This is so we only do few block moves of
// the data rather than constantly shuffle stuff around in the vector.
static const size_t kMaxItemsInTransitionList = 96;
static const size_t kResizeBigTransitionListTo = 64;
static_assert(kResizeBigTransitionListTo < kMaxItemsInTransitionList,
              "maxium number of items must be larger than we are resizing to");

VisitTracker::VisitTracker() {}

VisitTracker::~VisitTracker() {}

// This function is potentially slow because it may do up to two brute-force
// searches of the transitions list. This transitions list is kept to a
// relatively small number by CleanupTransitionList so it shouldn't be a big
// deal. However, if this ends up being noticable for performance, we may want
// to optimize lookup.
VisitID VisitTracker::GetLastVisit(ContextID context_id,
                                   int nav_entry_id,
                                   const GURL& url) {
  if (url.is_empty() || !context_id)
    return 0;

  auto i = contexts_.find(context_id);
  if (i == contexts_.end())
    return 0;  // We don't have any entries for this context.
  TransitionList& transitions = i->second;

  // Recall that a navigation entry ID is associated with a single session
  // history entry. In the case of automatically loaded iframes, many
  // visits/URLs can have the same navigation entry ID.
  //
  // We search backwards, starting at the current navigation entry ID, for the
  // referring URL. This won't always be correct. For example, if a render
  // process has the same page open in two different tabs, or even in two
  // different frames, we can get confused about which was which. We can have
  // the renderer report more precise referrer information in the future, but
  // this is a hard problem and doesn't affect much in terms of real-world
  // issues.
  //
  // We assume that the navigation entry IDs are increasing over time, so larger
  // IDs than the current input ID happened in the future (this will occur if
  // the user goes back). We can ignore future transitions because if you
  // navigate, go back, and navigate some more, we'd like to have one node with
  // two out edges in our visit graph.
  for (int i = static_cast<int>(transitions.size()) - 1; i >= 0; i--) {
    if (transitions[i].nav_entry_id <= nav_entry_id &&
        transitions[i].url == url) {
      // Found it.
      return transitions[i].visit_id;
    }
  }

  // We can't find the URL.
  return 0;
}

void VisitTracker::AddVisit(ContextID context_id,
                            int nav_entry_id,
                            const GURL& url,
                            VisitID visit_id) {
  TransitionList& transitions = contexts_[context_id];

  Transition t;
  t.url = url;
  t.nav_entry_id = nav_entry_id;
  t.visit_id = visit_id;
  transitions.push_back(t);

  CleanupTransitionList(&transitions);
}

void VisitTracker::ClearCachedDataForContextID(ContextID context_id) {
  contexts_.erase(context_id);
}


void VisitTracker::CleanupTransitionList(TransitionList* transitions) {
  if (transitions->size() <= kMaxItemsInTransitionList)
    return;  // Nothing to do.

  transitions->erase(transitions->begin(),
                     transitions->begin() + kResizeBigTransitionListTo);
}

}  // namespace history
