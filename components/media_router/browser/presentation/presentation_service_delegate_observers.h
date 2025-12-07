// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_OBSERVERS_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_OBSERVERS_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/presentation_service_delegate.h"

namespace media_router {

class PresentationServiceDelegateObservers {
 public:
  PresentationServiceDelegateObservers();
  virtual ~PresentationServiceDelegateObservers();

  // Registers an observer associated with frame with |render_process_id|
  // and |render_frame_id| with this class to listen for updates.
  // This class does not own the observer.
  // It is an error to add an observer if there is already an observer for that
  // frame.
  virtual void AddObserver(
      int render_process_id,
      int render_frame_id,
      content::PresentationServiceDelegate::Observer* observer);

  // Unregisters the observer associated with the frame with |render_process_id|
  // and |render_frame_id|.
  // The observer will no longer receive updates.
  virtual void RemoveObserver(int render_process_id, int render_frame_id);

 private:
  std::map<
      content::GlobalRenderFrameHostId,
      raw_ptr<content::PresentationServiceDelegate::Observer, CtnExperimental>>
      observers_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_OBSERVERS_H_
