// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace system_media_controls {

// Interface to observe events on the SystemMediaControls.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsObserver
    : public base::CheckedObserver {
 public:
  // Called when the service has completed setup. Also called when an observer
  // is added if the service is already set up.
  virtual void OnServiceReady() = 0;

  // Called when the observer should handle the given control.
  virtual void OnNext() = 0;
  virtual void OnPrevious() = 0;
  virtual void OnPlay() = 0;
  virtual void OnPause() = 0;
  virtual void OnPlayPause() = 0;
  virtual void OnStop() = 0;

 protected:
  ~SystemMediaControlsObserver() override = default;
};

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_OBSERVER_H_
