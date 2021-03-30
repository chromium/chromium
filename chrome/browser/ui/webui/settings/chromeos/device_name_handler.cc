// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_name_handler.h"

#include <string>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/chromeos/device_name_store.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

void DeviceNameHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceNameMetadata",
      base::BindRepeating(&DeviceNameHandler::HandleGetDeviceNameMetadata,
                          base::Unretained(this)));
}

void DeviceNameHandler::HandleGetDeviceNameMetadata(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  base::DictionaryValue metadata;
  metadata.SetString("deviceName",
                     DeviceNameStore::GetInstance()->GetDeviceName());

  ResolveJavascriptCallback(base::Value(callback_id), metadata);
}

}  // namespace settings
}  // namespace chromeos
