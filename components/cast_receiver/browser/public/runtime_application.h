// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_

#include <ostream>
#include <string>

#include "base/functional/callback.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"

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

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. The |callback| will be called indicating if the operation
  // succeeded or not. If Load fails, |this| should be destroyed since it's
  // not necessarily valid to retry Load with a new request.
  virtual void Load(StatusCallback callback) = 0;

  // Called to launch the application. The |callback| will be called
  // indicating if the operation succeeded or not.
  virtual void Launch(StatusCallback callback) = 0;

  // Called to stop the application. The |callback| will be called indicating
  // if the operation succeeded or not.
  virtual void Stop(StatusCallback callback) = 0;

  // Sets URL rewrite rules.
  virtual void SetUrlRewriteRules(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) = 0;

  // Sets media playback state.
  virtual void SetMediaBlocking(bool load_blocked, bool start_blocked) = 0;

  // Sets visibility state.
  virtual void SetVisibility(bool is_visible) = 0;

  // Sets touch input.
  virtual void SetTouchInputEnabled(bool enabled) = 0;
};

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app);

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_H_
