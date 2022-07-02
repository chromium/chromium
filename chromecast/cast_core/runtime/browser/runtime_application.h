// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_

#include <string>

#include "base/callback.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.grpc.pb.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebContents;

// This represents an application that can be hosted by RuntimeService.  Its
// lifecycle is very simple: Load() -> Launch() -> Destruction.  Implementations
// of this interface will additionally communicate over various gRPC interfaces.
// For example, Launch needs to respond with SetApplicationStatus.
class RuntimeApplication {
 public:
  using StatusCallback = base::OnceCallback<void(grpc::Status)>;

  RuntimeApplication() = default;
  virtual ~RuntimeApplication() = 0;

  // NOTE: These fields are the empty string until after Load().
  virtual const cast::common::ApplicationConfig& GetAppConfig() const = 0;

  // NOTE: These fields are the empty string until after Load().
  virtual const std::string& GetCastSessionId() const = 0;

  // Returns the root instance of CastWebContents.
  virtual CastWebContents* GetCastWebContents() = 0;

  // Returns the Cast media service endpoint for MZ.
  virtual const std::string& GetCastMediaServiceEndpoint() const = 0;

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. The |callback| will be called indicating if the operation
  // succeeded or not. If Load fails, |this| should be destroyed since it's not
  // necessarily valid to retry Load with a new |request|.
  virtual void Load(cast::runtime::LoadApplicationRequest request,
                    StatusCallback callback) = 0;

  // Called to launch the application. The application will indicate that it is
  // started by calling SetApplicationStatus back over gRPC. The |callback| will
  // be called indicating if the operation succeeded or not. If Load fails,
  // |this| should be destroyed since it's not necessarily valid to retry Load
  // with a new |request|.
  virtual void Launch(cast::runtime::LaunchApplicationRequest request,
                      StatusCallback callback) = 0;

  // Returns whether this instance is associated with cast streaming.
  virtual bool IsStreamingApplication() const = 0;
};

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_
