// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_LIST_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_LIST_H_

#include "base/observer_list.h"
#include "base/supports_user_data.h"

namespace content {
class RenderFrame;
}

namespace prerender {

class PrerenderObserver;

class PrerenderObserverList : public base::SupportsUserData::Data {
 public:
  static void AddObserverForFrame(content::RenderFrame* render_frame,
                                  PrerenderObserver* observer);

  static void RemoveObserverForFrame(content::RenderFrame* render_frame,
                                     PrerenderObserver* observer);

  static void SetIsPrerenderingForFrame(content::RenderFrame* render_frame,
                                        bool is_prerendering);

  PrerenderObserverList(const PrerenderObserverList&) = delete;
  PrerenderObserverList& operator=(const PrerenderObserverList&) = delete;
  ~PrerenderObserverList() override;

 private:
  PrerenderObserverList();

  void AddObserver(PrerenderObserver* observer);

  // Returns true if |prerender_observers_| is empty.
  bool RemoveObserver(PrerenderObserver* observer);

  void SetIsPrerendering(bool is_prerendering);

  // All the registered observers for prerender.
  base::ObserverList<prerender::PrerenderObserver> prerender_observers_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_OBSERVER_LIST_H_
