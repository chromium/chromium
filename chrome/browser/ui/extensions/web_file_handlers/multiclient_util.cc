// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/web_file_handlers/multiclient_util.h"

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

std::vector<apps::AppLaunchParams>
GetLaunchParamsIfLaunchTypeEqualsMultipleClients(
    const WebFileHandler& handler,
    const apps::AppLaunchParams& params,
    Profile* profile,
    const Extension& extension) {
  // Find the matching file_handler definition based on the handler action.
  if (handler.file_handler.action != params.intent->activity_name) {
    return {};
  }

  // Determine if this is single-client (default) or multiple-clients.
  if (handler.GetLaunchType() != WebFileHandler::LaunchType::kMultipleClients) {
    return {};
  }

  // Open a new window with a tab for each file being opened.
  std::vector<apps::AppLaunchParams> app_launch_params_list;
  for (const auto& file : params.launch_files) {
    std::vector<base::FilePath> files({file});

    // Clone intent to clone params.
    apps::IntentPtr intent =
        std::make_unique<apps::Intent>(params.intent->action);
    intent->mime_type = params.intent->mime_type;
    intent->url = params.intent->url;
    intent->activity_name = params.intent->activity_name;
    apps::AppLaunchParams file_params(params.app_id, params.container,
                                      params.disposition, params.launch_source,
                                      params.display_id, files, intent);
    app_launch_params_list.emplace_back(std::move(file_params));
  }
  return app_launch_params_list;
}

std::vector<apps::AppLaunchParams> CheckForMultiClientLaunchSupport(
    const Extension* extension,
    Profile* profile,
    const WebFileHandlersInfo& handlers,
    const apps::AppLaunchParams& params) {
  // Find a matching manifest file handler action for the intent. If there's a
  // match, return early with the list of AppLaunchParams.
  std::vector<apps::AppLaunchParams> app_launch_params_list;
  for (const auto& handler : handlers) {
    app_launch_params_list = GetLaunchParamsIfLaunchTypeEqualsMultipleClients(
        handler, params, profile, *extension);
    if (!app_launch_params_list.empty()) {
      return app_launch_params_list;
    }
  }

  // Multi-client wasn't detected, so this is treated as single-client.
  return std::vector<apps::AppLaunchParams>();
}

}  // namespace extensions
