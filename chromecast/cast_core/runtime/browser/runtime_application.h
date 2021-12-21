// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_

#include <string>

#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.grpc.pb.h"
#include "url/gurl.h"

namespace url_rewrite {
class UrlRequestRewriteRulesManager;
}

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
  const cast::common::ApplicationConfig& app_config() const {
    return app_config_;
  }

  // NOTE: These fields are the empty string until after Load().
  const std::string& cast_session_id() const { return cast_session_id_; }

  // NOTE: This is the empty string until after Launch().
  const std::string& cast_media_service_grpc_endpoint() const {
    return cast_media_service_grpc_endpoint_;
  }

  // NOTE: This is the empty string until after Launch().
  const GURL& app_url() const { return app_url_; }

  // NOTE: These fields are the empty string until after Load().
  virtual url_rewrite::UrlRequestRewriteRulesManager*
  GetUrlRewriteRulesManager() = 0;

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. This should return true if the load was successful and it's
  // valid to call Launch, false otherwise.  If Load fails, |this| should be
  // destroyed since it's not necessarily valid to retry Load with a new
  // |request|.
  virtual bool Load(const cast::runtime::LoadApplicationRequest& request) = 0;

  // Called to launch the application.  The application will indicate that it is
  // started by calling SetApplicationStatus back over gRPC.  This should return
  // true if initial processing of |request| succeeded, false otherwise.
  virtual bool Launch(
      const cast::runtime::LaunchApplicationRequest& request) = 0;

 protected:
  void set_application_config(cast::common::ApplicationConfig app_config) {
    app_config_ = std::move(app_config);
  }

  void set_cast_session_id(std::string cast_session_id) {
    cast_session_id_ = std::move(cast_session_id);
  }

  void set_cast_media_service_grpc_endpoint(
      std::string cast_media_service_grpc_endpoint) {
    cast_media_service_grpc_endpoint_ =
        std::move(cast_media_service_grpc_endpoint);
  }

  void set_app_url(GURL app_url) { app_url_ = std::move(app_url); }

 private:
  cast::common::ApplicationConfig app_config_;
  std::string cast_session_id_;
  std::string cast_media_service_grpc_endpoint_;
  GURL app_url_;
};

std::ostream& operator<<(std::ostream& os, const RuntimeApplication& app);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_
