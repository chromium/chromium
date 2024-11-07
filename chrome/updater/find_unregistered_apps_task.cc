// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/find_unregistered_apps_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/util/util.h"

namespace updater {

FindUnregisteredAppsTask::FindUnregisteredAppsTask(
    scoped_refptr<Configurator> config,
    UpdaterScope scope)
    : config_(config), scope_(scope) {}

FindUnregisteredAppsTask::~FindUnregisteredAppsTask() = default;

void FindUnregisteredAppsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MigrateLegacyUpdaters(
      scope_, base::BindRepeating(
                  [](scoped_refptr<PersistedData> persisted_data,
                     const RegistrationRequest& req) {
                    if (!base::Contains(persisted_data->GetAppIds(),
                                        base::ToLowerASCII(req.app_id))) {
                      persisted_data->RegisterApp(req);
                    }
                  },
                  config_->GetUpdaterPersistedData()));
  std::move(callback).Run();
}

}  // namespace updater
