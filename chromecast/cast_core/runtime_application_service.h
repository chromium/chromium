// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_SERVICE_H_

#include <string>

#include "third_party/openscreen/src/cast/cast_core/api/runtime/runtime_service.grpc.pb.h"

namespace chromecast {

// This represents an application that can be hosted by RuntimeService.  Its
// lifecycle is very simple: Load() -> Launch() -> Destruction.  Implementations
// of this interface will additionally communicate over various gRPC interfaces.
// For example, Launch needs to respond with SetApplicationStatus.
class RuntimeApplicationService {
 public:
  virtual ~RuntimeApplicationService() = 0;

  // NOTE: These fields are the empty string until after Load().
  const std::string& app_id() const { return app_id_; }
  const std::string& cast_session_id() const { return cast_session_id_; }

  // NOTE: This is the empty string until after Launch().
  const std::string& cast_media_service_grpc_endpoint() const {
    return cast_media_service_grpc_endpoint_;
  }

  // Called before Launch() to perform any pre-launch loading that is necessary.
  // This should return true if the load was successful and it's valid to call
  // Launch, false otherwise.  If Load fails, |this| should be destroyed since
  // it's not necessarily valid to retry Load with a new |request|.
  virtual bool Load(const cast::runtime::LoadApplicationRequest& request) = 0;

  // Called to launch the application.  The application will indicate that it is
  // started by calling SetApplicationStatus back over gRPC.  This should return
  // true if initial processing of |request| succeeded, false otherwise.
  virtual bool Launch(
      const cast::runtime::LaunchApplicationRequest& request) = 0;

 protected:
  std::string app_id_;
  std::string cast_session_id_;
  std::string cast_media_service_grpc_endpoint_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_SERVICE_H_
