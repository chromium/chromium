// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_DISPATCHER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/cast_receiver/browser/public/application_config.h"

namespace cast_receiver {

class RuntimeApplication;

// This class is responsible for creating new RuntimeApplication instances for
// and managing all existing instances.
//
// |TEmbedderApplication| must implement EmbedderApplication.
template <typename TEmbedderApplication>
class RuntimeApplicationDispatcher {
 public:
  // Creates a new instance of TEmbedderApplication. The provided instance
  // of RuntimeApplication is expected to persist for the duration of the
  // created types lifetime.
  using EmbedderApplicationFactory =
      base::OnceCallback<std::unique_ptr<TEmbedderApplication>(
          std::unique_ptr<RuntimeApplication>)>;

  virtual ~RuntimeApplicationDispatcher() = default;

  // Creates an application of |TEmbedderApplication| type and adds to
  // the |loaded_apps_| list.
  virtual TEmbedderApplication* CreateApplication(
      std::string session_id,
      cast_receiver::ApplicationConfig app_config,
      EmbedderApplicationFactory factory) = 0;

  // Returns an existing application with session id as specified or nullptr if
  // no such application exists.
  virtual TEmbedderApplication* GetApplication(
      const std::string& session_id) = 0;

  // Destroys an existing application with session id as specified and returns
  // the removed instance.
  virtual std::unique_ptr<TEmbedderApplication> DestroyApplication(
      const std::string& session_id) = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_RUNTIME_APPLICATION_DISPATCHER_H_
