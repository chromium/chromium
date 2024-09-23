// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/find_unregistered_apps_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
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
                  [](const std::vector<std::string>& known_apps,
                     scoped_refptr<PersistedData> persisted_data,
                     const RegistrationRequest& req) {
                    if (base::ranges::find_if(
                            known_apps, [&](const std::string& known_id) {
                              return base::EqualsCaseInsensitiveASCII(
                                  known_id, req.app_id);
                            }) == known_apps.end()) {
                      persisted_data->RegisterApp(req);
                    }
                  },
                  config_->GetUpdaterPersistedData()->GetAppIds(),
                  config_->GetUpdaterPersistedData()));
  std::move(callback).Run();
}

}  // namespace updater
