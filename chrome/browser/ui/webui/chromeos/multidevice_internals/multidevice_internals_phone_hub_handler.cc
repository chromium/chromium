// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_phone_hub_handler.h"

#include "ash/public/cpp/system_tray.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"

namespace chromeos {
namespace multidevice {

MultidevicePhoneHubHandler::MultidevicePhoneHubHandler() = default;

MultidevicePhoneHubHandler::~MultidevicePhoneHubHandler() {
  if (fake_phone_hub_manager_)
    SetSystemPhoneHubManagerEnabled();
}

void MultidevicePhoneHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setFakePhoneHubManagerEnabled",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetFakePhoneHubManagerEnabled,
          base::Unretained(this)));
}

void MultidevicePhoneHubHandler::SetSystemPhoneHubManagerEnabled() {
  fake_phone_hub_manager_.reset();
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::phonehub::PhoneHubManager* phone_hub_manager =
      chromeos::phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  ash::SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);
}

void MultidevicePhoneHubHandler::SetFakePhoneHubManagerEnabled() {
  fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
  ash::SystemTray::Get()->SetPhoneHubManager(fake_phone_hub_manager_.get());
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneHubManagerEnabled(
    const base::ListValue* args) {
  AllowJavascript();
  bool enabled = false;
  CHECK(args->GetBoolean(0, &enabled));
  if (enabled) {
    SetFakePhoneHubManagerEnabled();
    return;
  }
  SetSystemPhoneHubManagerEnabled();
}

}  // namespace multidevice
}  // namespace chromeos
