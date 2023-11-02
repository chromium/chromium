// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace base {
class TimeDelta;
}

namespace system_media_controls {

// Interface to observe events on the SystemMediaControls.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsObserver
    : public base::CheckedObserver {
 public:
  // Called when the service has completed setup. Also called when an observer
  // is added if the service is already set up.
  virtual void OnServiceReady() {}

  // Called when the observer should handle the given control.
  virtual void OnNext() {}
  virtual void OnPrevious() {}
  virtual void OnPlay() {}
  virtual void OnPause() {}
  virtual void OnPlayPause() {}
  virtual void OnStop() {}
  virtual void OnSeek(const base::TimeDelta& time) {}
  virtual void OnSeekTo(const base::TimeDelta& time) {}

 protected:
  ~SystemMediaControlsObserver() override = default;
};

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_
