// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_STATE_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_STATE_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/cast_receiver/browser/public/runtime_application_state.h"

namespace cast_receiver {

// Provides accessors for information about a RuntimeApplication.
//
// TODO(crbug.com/1379082): Rename this class to RuntimeApplication.
class RuntimeApplicationState {
 public:
  virtual ~RuntimeApplicationState();

  // Returns the display name of the application.
  virtual const std::string& GetDisplayName() const = 0;

  // Returns the application ID of the application.
  virtual const std::string& GetAppId() const = 0;

  // Returns the session id for this cast session.
  virtual const std::string& GetCastSessionId() const = 0;

  // Returns whether this instance is associated with cast streaming.
  virtual bool IsStreamingApplication() const = 0;

  // Returns whether this application is currently running.
  virtual bool IsApplicationRunning() const = 0;
};

std::ostream& operator<<(std::ostream& os,
                         const RuntimeApplicationState& app_state);

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_STATE_H_
