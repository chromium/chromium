// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/cast_receiver/browser/public/application_config.h"

namespace chromecast {

class RuntimeApplicationBase;

template <typename TRuntimeApplicationPlatform>
class RuntimeApplicationDispatcher {
 public:
  using RuntimeApplicationPlatformFactory =
      base::OnceCallback<std::unique_ptr<TRuntimeApplicationPlatform>(
          std::unique_ptr<RuntimeApplicationBase>)>;

  virtual ~RuntimeApplicationDispatcher() = default;

  // Creates an application of |TRuntimeApplicationPlatform| type and adds to
  // the |loaded_apps_| list.
  virtual TRuntimeApplicationPlatform* CreateApplication(
      std::string session_id,
      cast_receiver::ApplicationConfig app_config,
      RuntimeApplicationPlatformFactory factory) = 0;

  // Returns an existing application or nullptr.
  virtual TRuntimeApplicationPlatform* GetApplication(
      const std::string& session_id) = 0;

  // Destroys an existing application and returns its pointer for possible
  // post-processing.
  virtual std::unique_ptr<TRuntimeApplicationPlatform> DestroyApplication(
      const std::string& session_id) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
