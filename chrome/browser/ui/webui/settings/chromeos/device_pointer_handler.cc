// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

PointerHandler::PointerHandler() {}

PointerHandler::~PointerHandler() {}

void PointerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializePointerSettings",
      base::BindRepeating(&PointerHandler::HandleInitialize,
                          base::Unretained(this)));
}

void PointerHandler::OnJavascriptAllowed() {
  if (!pointer_device_observer_) {
    pointer_device_observer_ =
        std::make_unique<system::PointerDeviceObserver>();
    pointer_device_observer_->Init();
  }

  pointer_device_observer_->AddObserver(this);
}

void PointerHandler::OnJavascriptDisallowed() {
  pointer_device_observer_->RemoveObserver(this);
}

void PointerHandler::TouchpadExists(bool exists) {
  FireWebUIListener("has-touchpad-changed", base::Value(exists));
}

void PointerHandler::MouseExists(bool exists) {
  FireWebUIListener("has-mouse-changed", base::Value(exists));
}

void PointerHandler::PointingStickExists(bool exists) {
  FireWebUIListener("has-pointing-stick-changed", base::Value(exists));
}

void PointerHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();

  // CheckDevices() results in TouchpadExists() and MouseExists() being called.
  pointer_device_observer_->CheckDevices();
}

}  // namespace settings
}  // namespace chromeos
