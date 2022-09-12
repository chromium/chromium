// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher_platform.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"

namespace cast_receiver {
class ApplicationClient;
}

namespace chromecast {

class CastWebService;
class RuntimeApplication;

class RuntimeApplicationDispatcher final
    : public RuntimeApplicationDispatcherPlatform::Client {
 public:
  using PlatformFactory =
      base::OnceCallback<std::unique_ptr<RuntimeApplicationDispatcherPlatform>(
          RuntimeApplicationDispatcherPlatform::Client& client,
          CastWebService*)>;

  // |application_client| is expected to persist for the lifetime of this
  // instance.
  RuntimeApplicationDispatcher(
      PlatformFactory platform_factory,
      CastWebService* web_service,
      cast_receiver::ApplicationClient& application_client);
  ~RuntimeApplicationDispatcher() override;

  // Starts and stops the runtime service, including the gRPC completion queue.
  bool Start();
  void Stop();

 private:
  // RuntimeApplicationDispatcherPlatform::Client implementation:
  void LoadApplication(
      cast::runtime::LoadApplicationRequest request,
      StatusCallback callback,
      RuntimeApplicationPlatform::Factory runtime_application_factory) override;
  void LaunchApplication(cast::runtime::LaunchApplicationRequest request,
                         StatusCallback callback) override;
  std::unique_ptr<RuntimeApplication> StopApplication(
      std::string session_id) override;
  bool HasApplication(const std::string& session_id) override;

  // Calls |callback| then resets the app if |status| is a failure.
  void OnApplicationLaunching(std::string session_id,
                              StatusCallback callback,
                              cast_receiver::Status);

  std::unique_ptr<RuntimeApplicationDispatcherPlatform> platform_;

  CastWebService* const web_service_;

  base::raw_ref<cast_receiver::ApplicationClient> const application_client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::flat_map<std::string, std::unique_ptr<RuntimeApplication>> loaded_apps_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationDispatcher> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
