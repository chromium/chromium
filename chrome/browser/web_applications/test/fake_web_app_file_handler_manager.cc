// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

FakeWebAppFileHandlerManager::FakeWebAppFileHandlerManager(Profile* profile)
    : WebAppFileHandlerManager(profile) {}

FakeWebAppFileHandlerManager::~FakeWebAppFileHandlerManager() = default;

const apps::FileHandlers* FakeWebAppFileHandlerManager::GetAllFileHandlers(
    const webapps::AppId& app_id) const {
  if (base::Contains(file_handlers_, app_id))
    return &file_handlers_.at(app_id);

  return WebAppFileHandlerManager::GetAllFileHandlers(app_id);
}

bool FakeWebAppFileHandlerManager::IsDisabledForTesting() {
  return true;
}

void FakeWebAppFileHandlerManager::InstallFileHandler(
    const webapps::AppId& app_id,
    const GURL& action,
    const AcceptMap& accept,
    absl::optional<apps::FileHandler::LaunchType> launch_type,
    bool enable) {
  if (!base::Contains(file_handlers_, app_id))
    file_handlers_[app_id] = apps::FileHandlers();

  apps::FileHandler file_handler;
  file_handler.action = action;
  if (launch_type)
    file_handler.launch_type = *launch_type;

  for (const auto& it : accept) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = it.first;
    accept_entry.file_extensions = it.second;
    file_handler.accept.push_back(accept_entry);
  }

  file_handlers_[app_id].push_back(file_handler);

  if (enable) {
    base::RunLoop run_loop;
    EnableAndRegisterOsFileHandlers(
        app_id, base::BindLambdaForTesting([&](Result result) {
          DCHECK_EQ(result, Result::kOk);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace web_app
