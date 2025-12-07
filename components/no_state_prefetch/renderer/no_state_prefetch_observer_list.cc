// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/no_state_prefetch_observer_list.h"

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_observer.h"
#include "content/public/renderer/render_frame.h"

namespace prerender {

namespace {

// Key used to attach the handler to the RenderFrame.
const char kNoStatePrefetchObserverListKey[] =
    "kNoStatePrefetchObserverListKey";

}  // namespace

// static
void NoStatePrefetchObserverList::AddObserverForFrame(
    content::RenderFrame* render_frame,
    NoStatePrefetchObserver* observer) {
  auto* observer_list = static_cast<NoStatePrefetchObserverList*>(
      render_frame->GetUserData(kNoStatePrefetchObserverListKey));
  if (!observer_list) {
    observer_list = new NoStatePrefetchObserverList();
    render_frame->SetUserData(kNoStatePrefetchObserverListKey,
                              base::WrapUnique(observer_list));
  }

  observer_list->AddObserver(observer);
}

// static
void NoStatePrefetchObserverList::RemoveObserverForFrame(
    content::RenderFrame* render_frame,
    NoStatePrefetchObserver* observer) {
  auto* observer_list = static_cast<NoStatePrefetchObserverList*>(
      render_frame->GetUserData(kNoStatePrefetchObserverListKey));
  DCHECK(observer_list);

  // Delete the NoStatePrefetchObserverList instance when the last observer is
  // removed.
  if (observer_list->RemoveObserver(observer)) {
    render_frame->RemoveUserData(kNoStatePrefetchObserverListKey);
  }
}

// static
void NoStatePrefetchObserverList::SetIsNoStatePrefetchingForFrame(
    content::RenderFrame* render_frame,
    bool is_no_state_prefetching) {
  auto* observer_list = static_cast<NoStatePrefetchObserverList*>(
      render_frame->GetUserData(kNoStatePrefetchObserverListKey));
  if (observer_list) {
    observer_list->SetIsNoStatePrefetching(is_no_state_prefetching);
  }
}

NoStatePrefetchObserverList::NoStatePrefetchObserverList() = default;
NoStatePrefetchObserverList::~NoStatePrefetchObserverList() = default;

void NoStatePrefetchObserverList::AddObserver(
    NoStatePrefetchObserver* observer) {
  no_state_prefetch_observers_.AddObserver(observer);
}

bool NoStatePrefetchObserverList::RemoveObserver(
    NoStatePrefetchObserver* observer) {
  no_state_prefetch_observers_.RemoveObserver(observer);
  return no_state_prefetch_observers_.empty();
}

void NoStatePrefetchObserverList::SetIsNoStatePrefetching(
    bool is_no_state_prefetching) {
  for (auto& observer : no_state_prefetch_observers_) {
    observer.SetIsNoStatePrefetching(is_no_state_prefetching);
  }
}

}  // namespace prerender
