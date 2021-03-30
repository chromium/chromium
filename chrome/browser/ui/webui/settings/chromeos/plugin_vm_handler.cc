// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace settings {

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
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  bool requires_relaunch =
      plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
          ->IsRelaunchNeededForNewPermissions();
  ResolveJavascriptCallback(
      /*callback_id=*/base::Value(args->GetList()[0].GetString()),
      base::Value(requires_relaunch));
}

void PluginVmHandler::HandleRelaunchPluginVm(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)
      ->RelaunchPluginVm();
}

}  // namespace settings
}  // namespace chromeos
