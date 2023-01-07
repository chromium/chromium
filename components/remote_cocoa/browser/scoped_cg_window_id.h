// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/remote_cocoa/browser/remote_cocoa_browser_export.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace remote_cocoa {

// A registry of the CGWindowIDs for the remote cocoa windows created in any
// process (the browser and also the renderer). This can be used to look up
// the compositing frame sink id for efficient video capture. This class may
// only be accessed on the UI thread.
class REMOTE_COCOA_BROWSER_EXPORT ScopedCGWindowID final {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // This is invoked in the ScopedCGWindowID destructor, after all of its
    // weak pointers have been destroyed.
    virtual void OnScopedCGWindowIDDestroyed(uint32_t cg_window_id) {}
    virtual void OnScopedCGWindowIDMouseMoved(
        uint32_t cg_window_id,
        const gfx::PointF& location_in_window_dips,
        const gfx::Size& window_size_dips) {}
  };
  ScopedCGWindowID(uint32_t cg_window_id,
                   const viz::FrameSinkId& frame_sink_id);
  ~ScopedCGWindowID();
  ScopedCGWindowID(const ScopedCGWindowID&) = delete;
  ScopedCGWindowID& operator=(const ScopedCGWindowID&) = delete;

  // Add and remove an observer. This class will automatically remove all
  // observers after invalidating its weak pointers during its destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Inform frame sink capturers of where the mouse is, so that they can draw
  // a cursor.
  void OnMouseMoved(const gfx::PointF& location_in_window_dips,
                    const gfx::Size& window_size_dips);

  // Query the frame sink id for this window, which can be used for optimized
  // video capture.
  const viz::FrameSinkId& GetFrameSinkId() const { return frame_sink_id_; }

  // Look up this structure corresponding to a given CGWindowID. The returned
  // weak pointer may only be accessed on the UI thread, and will be invalidated
  // before it calls OnScopedCGWindowIDDestroying on its observers.
  static base::WeakPtr<ScopedCGWindowID> Get(uint32_t cg_window_id);

 private:
  THREAD_CHECKER(thread_checker_);
  const uint32_t cg_window_id_;
  const viz::FrameSinkId frame_sink_id_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<ScopedCGWindowID> weak_factory_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_
