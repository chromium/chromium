// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_LIST_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_LIST_H_

#include "base/observer_list.h"
#include "base/supports_user_data.h"

namespace content {
class RenderFrame;
}

namespace prerender {

class NoStatePrefetchObserver;

class NoStatePrefetchObserverList : public base::SupportsUserData::Data {
 public:
  static void AddObserverForFrame(content::RenderFrame* render_frame,
                                  NoStatePrefetchObserver* observer);

  static void RemoveObserverForFrame(content::RenderFrame* render_frame,
                                     NoStatePrefetchObserver* observer);

  static void SetIsNoStatePrefetchingForFrame(
      content::RenderFrame* render_frame,
      bool is_no_state_prefetching);

  NoStatePrefetchObserverList(const NoStatePrefetchObserverList&) = delete;
  NoStatePrefetchObserverList& operator=(const NoStatePrefetchObserverList&) =
      delete;
  ~NoStatePrefetchObserverList() override;

 private:
  NoStatePrefetchObserverList();

  void AddObserver(NoStatePrefetchObserver* observer);

  // Returns true if |no_state_prefetch_observers_| is empty.
  bool RemoveObserver(NoStatePrefetchObserver* observer);

  void SetIsNoStatePrefetching(bool is_no_state_prefetching);

  // All the registered observers for prefetch.
  base::ObserverList<prerender::NoStatePrefetchObserver>
      no_state_prefetch_observers_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_OBSERVER_LIST_H_
