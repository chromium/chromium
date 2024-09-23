// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_HANDLE_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "url/gurl.h"

namespace prerender {

class NoStatePrefetchContents;

// A class representing a running prefetch to a client of the
// NoStatePrefetchManager. Methods on NoStatePrefetchManager which start
// prefetch return a caller-owned NoStatePrefetchHandle* to the client (or
// NULL if they are unable to start prefetch). Calls on the handle of prefetch
// that is not running at no-ops. Destroying a handle before prefetch starts
// will prevent it from ever starting. Destroying a handle while prefetch is
// running will stop the prefetch, without making any calls to the observer.
class NoStatePrefetchHandle : public NoStatePrefetchContents::Observer {
 public:
  class Observer {
   public:
    // Signals that the prefetch has stopped running.
    virtual void OnPrefetchStop(NoStatePrefetchHandle* handle) = 0;

   protected:
    Observer();
    virtual ~Observer();
  };

  NoStatePrefetchHandle(const NoStatePrefetchHandle&) = delete;
  NoStatePrefetchHandle& operator=(const NoStatePrefetchHandle&) = delete;

  // Before calling the destructor, the caller must invalidate the handle by
  // calling either OnNavigateAway or OnCancel.
  ~NoStatePrefetchHandle() override;

  void SetObserver(Observer* observer);

  // The launcher is navigating away from the context that launched this
  // prefetch. The prefetch will likely stay alive briefly though, in case
  // we are going through a redirect chain that will target it.
  void OnNavigateAway();

  // The launcher has taken explicit action to remove this prefetch (for
  // instance, removing a link element from a document). This call invalidates
  // the handle. If the prefetch handle is already invalid, this call does
  // nothing.
  void OnCancel();

  // True if this prefetch is currently active.
  bool IsPrefetching() const;

  // True if we started prefetch, and it has finished loading.
  bool IsFinishedLoading() const;

  NoStatePrefetchContents* contents() const;

  // Returns whether this NoStatePrefetchHandle represents the same prefetch as
  // the other NoStatePrefetchHandle object specified.
  bool RepresentingSamePrefetchAs(NoStatePrefetchHandle* other) const;

 private:
  friend class NoStatePrefetchManager;

  explicit NoStatePrefetchHandle(
      NoStatePrefetchManager::NoStatePrefetchData* prefetch_data);

  // From NoStatePrefetchContents::Observer:
  void OnPrefetchStop(
      NoStatePrefetchContents* no_state_prefetch_contents) override;

  raw_ptr<Observer> observer_;

  base::WeakPtr<NoStatePrefetchManager::NoStatePrefetchData> prefetch_data_;
  base::WeakPtrFactory<NoStatePrefetchHandle> weak_ptr_factory_{this};
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_HANDLE_H_
