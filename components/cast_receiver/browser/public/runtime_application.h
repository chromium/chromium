// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_

#include <ostream>
#include <string>

#include "base/callback.h"
#include "components/cast_receiver/common/public/status.h"

namespace cast_receiver {

// Provides accessors for information about an application running in this
// runtime.
class RuntimeApplication {
 public:
  using StatusCallback = base::OnceCallback<void(cast_receiver::Status)>;

  virtual ~RuntimeApplication();

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

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app);

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_
