// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_STATE_OBSERVER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_STATE_OBSERVER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace cast_receiver {

class RuntimeApplication;

// Provides callbacks associated with changes to the state
class ApplicationStateObserver : public base::CheckedObserver {
 public:
  ~ApplicationStateObserver() override = default;

  // Called when the foreground application changes. |app| is a valid pointer
  // when application is brought to the foreground and nullptr in all other
  // cases.
  virtual void OnForegroundApplicationChanged(RuntimeApplication* app) = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_STATE_OBSERVER_H_
