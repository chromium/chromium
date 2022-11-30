// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_tracker.h"

#include <stddef.h>

#include <algorithm>

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
// deal. However, if this ends up being noticeable for performance, we may want
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
  for (int j = static_cast<int>(transitions.size()) - 1; j >= 0; j--) {
    if (transitions[j].nav_entry_id <= nav_entry_id &&
        transitions[j].url == url) {
      // Found it.
      return transitions[j].visit_id;
    }
  }

  // We can't find the URL.
  return 0;
}

void VisitTracker::AddVisit(ContextID context_id,
                            int nav_entry_id,
                            const GURL& url,
                            VisitID visit_id) {
  if (IsEmpty()) {
    // First visit, reset `visit_id_range_if_sorted_` to indicate visit ids are
    // sorted.
    visit_id_range_if_sorted_ = {visit_id, visit_id};
  } else if (are_transition_lists_sorted() &&
             visit_id > visit_id_range_if_sorted_->max_id) {
    // Common case, visit ids increase.
    visit_id_range_if_sorted_->max_id = visit_id;
  } else {
    // A visit was added with an id in the existing range. This generally
    // happens in two scenarios:
    // . Recent history was deleted.
    // . The ids wrapped.
    // These two scenarios are uncommon. Mark `visit_id_range_if_sorted_` as
    // invalid so this fallsback to brute force.
    visit_id_range_if_sorted_.reset();
  }

  TransitionList& transitions = contexts_[context_id];

  Transition t;
  t.url = url;
  t.nav_entry_id = nav_entry_id;
  t.visit_id = visit_id;
  transitions.push_back(t);

  CleanupTransitionList(&transitions);

#if DCHECK_IS_ON()
  // If are_transition_lists_sorted() is true, the ids should be sorted.
  DCHECK(!are_transition_lists_sorted() ||
         std::is_sorted(transitions.begin(), transitions.end(),
                        TransitionVisitIdComparator()));
#endif
}

void VisitTracker::RemoveVisitById(VisitID visit_id) {
  if (IsEmpty())
    return;

  if (visit_id_range_if_sorted_ &&
      (visit_id < visit_id_range_if_sorted_->min_id ||
       visit_id > visit_id_range_if_sorted_->max_id)) {
    return;
  }

  const Transition transition_for_search = {{}, 0, visit_id};
  for (auto& id_and_list_pair : contexts_) {
    TransitionList& transitions = id_and_list_pair.second;
    auto iter =
        FindTransitionListIteratorByVisitId(transitions, transition_for_search);
    if (iter != transitions.end()) {
      transitions.erase(iter);
      if (transitions.empty())
        contexts_.erase(id_and_list_pair.first);
      // visit-ids are unique. Once a match is found, stop.
      // See description of `visit_id_range_if_sorted_` for details on why it
      // is not recalculated here.
      return;
    }
  }
}

void VisitTracker::Clear() {
  contexts_.clear();
}

void VisitTracker::ClearCachedDataForContextID(ContextID context_id) {
  contexts_.erase(context_id);
  if (contexts_.empty())
    visit_id_range_if_sorted_.reset();
}

void VisitTracker::CleanupTransitionList(TransitionList* transitions) {
  if (transitions->size() <= kMaxItemsInTransitionList)
    return;  // Nothing to do.

  transitions->erase(transitions->begin(),
                     transitions->begin() + kResizeBigTransitionListTo);
  // See description of `visit_id_range_if_sorted_` for details on why it is not
  // recalculated here.
}

VisitTracker::TransitionList::const_iterator
VisitTracker::FindTransitionListIteratorByVisitId(
    const TransitionList& transitions,
    const Transition& transition_for_search) {
  if (!are_transition_lists_sorted()) {
    // If `transitions` are not sorted, then we can't use a binary search. This
    // is uncommon enough, that we fallback to brute force.
    for (auto iter = transitions.begin(); iter != transitions.end(); ++iter) {
      if (iter->visit_id == transition_for_search.visit_id)
        return iter;
    }
    return transitions.end();
  }
  auto iter =
      std::lower_bound(transitions.begin(), transitions.end(),
                       transition_for_search, TransitionVisitIdComparator());
  return iter != transitions.end() &&
                 iter->visit_id == transition_for_search.visit_id
             ? iter
             : transitions.end();
}

}  // namespace history
