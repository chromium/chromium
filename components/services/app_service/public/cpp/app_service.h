// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/launch_result.h"

namespace apps {

class AppRegistryCache;

// An interface representing the AppService service.
class COMPONENT_EXPORT(APP_SERVICE) AppService {
 public:
  virtual ~AppService();

  // Returns the cache registry of the apps for the user tied to this service.
  virtual apps::AppRegistryCache& AppRegistryCache() = 0;

  // Launches the app for the given `app_id`.
  //
  // - `event_flags` is a bitset of ui::EventFlags providing additional context
  // about the action which launches the app (e.g. a middle click indicating
  // opening a background tab).
  // - `launch_source` is the UI surface which is launching the app (e.g. shelf,
  // search box).
  // - `window_info` specifies the desired location of the new app window
  // (e.g. window bounds, display ID). If `window_info` is unspecified or
  // nullptr, the app publisher will position the new app window using its
  // default behavior (e.g. on the currently active display).
  //
  // Note: prefer using LaunchSystemWebAppAsync() for launching System Web Apps,
  // as that is robust to the choice of profile and avoids needing to specify an
  // app_id.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source) {
    Launch(app_id, event_flags, launch_source, nullptr);
  }
  virtual void Launch(const std::string& app_id,
                      int32_t event_flags,
                      apps::LaunchSource launch_source,
                      apps::WindowInfoPtr window_info) = 0;

  // Launches an app for the given `app_id`, passing `intent` to the app.
  //
  // - `event_flags` is a bitset of ui::EventFlags providing additional context
  // about the action which launches the app (e.g. a middle click indicating
  // opening a background tab).
  // - `launch_source` is the UI surface which is launching the app (e.g. shelf,
  // search box).
  // - `window_info` specifies the desired location of the new app window
  // (e.g. window bounds, display ID). If `window_info` is nullptr, the app
  // publisher will position the new app window using its default behavior (e.g.
  // on the currently active display).
  // - `callback` will be called with the result of the launch once it is
  // complete.
  virtual void LaunchAppWithIntent(const std::string& app_id,
                                   int32_t event_flags,
                                   IntentPtr intent,
                                   LaunchSource launch_source,
                                   WindowInfoPtr window_info,
                                   LaunchCallback callback) = 0;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_H_
