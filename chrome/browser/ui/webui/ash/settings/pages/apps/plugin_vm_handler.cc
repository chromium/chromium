// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/plugin_vm_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace ash::settings {

PluginVmHandler::PluginVmHandler(Profile* profile) : profile_(profile) {}

PluginVmHandler::~PluginVmHandler() = default;

void PluginVmHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "isRelaunchNeededForNewPermissions",
      base::BindRepeating(
          &PluginVmHandler::HandleIsRelaunchNeededForNewPermissions,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "relaunchPluginVm",
      base::BindRepeating(&PluginVmHandler::HandleRelaunchPluginVm,
                          base::Unretained(this)));
}

void PluginVmHandler::OnJavascriptAllowed() {}

void PluginVmHandler::OnJavascriptDisallowed() {}

void PluginVmHandler::HandleIsRelaunchNeededForNewPermissions(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  bool requires_relaunch =
      plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
          ->IsRelaunchNeededForNewPermissions();
  ResolveJavascriptCallback(
      /*callback_id=*/base::Value(args[0].GetString()),
      base::Value(requires_relaunch));
}

void PluginVmHandler::HandleRelaunchPluginVm(const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
      ->RelaunchPluginVm();
}

}  // namespace ash::settings
