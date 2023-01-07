// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"

namespace chromecast {

class RuntimeApplication;

// This class defines a wrapper around any platform-specific communication
// details required for functionality of a RuntimeApplicationDispatcher.
class RuntimeApplicationDispatcherPlatform {
 public:
  // Client used for executing commands in the runtime based on signals received
  // by the embedder implementing RuntimeApplicationDispatcherPlatform.
  class Client {
   public:
    // TODO(crbug.com/1360597): Add details of this failure to the new Status
    // object provided to these callback methods.
    using StatusCallback = base::OnceCallback<void(cast_receiver::Status)>;

    virtual ~Client() = default;

    // Returns whether this client has an application associated with the given
    // |session_id|.
    virtual bool HasApplication(const std::string& session_id) = 0;

    // Loads a new application with details as defined in |request|, using
    // |runtime_application_factory| to create the application's platform and
    // calling |callback| with the result of the operation upon completion.
    virtual void LoadApplication(
        cast::runtime::LoadApplicationRequest request,
        StatusCallback callback,
        RuntimeApplicationPlatform::Factory runtime_application_factory) = 0;

    // Launches an already loaded application with details as defined in
    // |request|, calling |callback| upon completion with the result of the
    // operation.
    virtual void LaunchApplication(
        cast::runtime::LaunchApplicationRequest request,
        StatusCallback callback) = 0;

    // Stops the previously loaded application with |session_id| as provided,
    // returning the associated RuntimeApplication instance to the caller.
    virtual std::unique_ptr<RuntimeApplication> StopApplication(
        std::string session_id) = 0;
  };

  virtual ~RuntimeApplicationDispatcherPlatform() = default;

  // Starts and stops the platform.
  virtual bool Start() = 0;
  virtual void Stop() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_H_
