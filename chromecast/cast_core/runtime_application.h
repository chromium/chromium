// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_H_

#include <string>

#include "third_party/cast_core/public/src/proto/runtime/runtime_service.grpc.pb.h"

namespace chromecast {

// This represents an application that can be hosted by RuntimeService.  Its
// lifecycle is very simple: Load() -> Launch() -> Destruction.  Implementations
// of this interface will additionally communicate over various gRPC interfaces.
// For example, Launch needs to respond with SetApplicationStatus.
class RuntimeApplication {
 public:
  RuntimeApplication();
  virtual ~RuntimeApplication() = 0;

  // NOTE: These fields are the empty string until after Load().
  const std::string& app_id() const { return app_id_; }
  const std::string& cast_session_id() const { return cast_session_id_; }
  const std::string& display_name() const { return display_name_; }

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
  void set_app_id(std::string app_id) { app_id_ = std::move(app_id); }

  void set_cast_session_id(std::string cast_session_id) {
    cast_session_id_ = std::move(cast_session_id);
  }

  void set_display_name(std::string display_name) {
    app_id_ = std::move(display_name);
  }

  void set_cast_media_service_grpc_endpoint(
      std::string cast_media_service_grpc_endpoint) {
    cast_media_service_grpc_endpoint_ =
        std::move(cast_media_service_grpc_endpoint);
  }

 private:
  std::string app_id_;
  std::string cast_session_id_;
  std::string display_name_;
  std::string cast_media_service_grpc_endpoint_;
};

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_APPLICATION_H_
