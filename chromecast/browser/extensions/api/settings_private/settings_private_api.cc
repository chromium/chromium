// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/settings_private/settings_private_api.h"

#include "base/values.h"

namespace extensions {
namespace cast {

namespace {

const char kErrorNotSupported[] = "This API is not supported on this platform.";

}  // namespace

SettingsPrivateSetPrefFunction::~SettingsPrivateSetPrefFunction() {}

ExtensionFunction::ResponseAction SettingsPrivateSetPrefFunction::Run() {
  return RespondNow(Error(kErrorNotSupported));
}

SettingsPrivateGetAllPrefsFunction::~SettingsPrivateGetAllPrefsFunction() {}

ExtensionFunction::ResponseAction SettingsPrivateGetAllPrefsFunction::Run() {
  return RespondNow(OneArgument(base::Value(base::Value::Type::LIST)));
}

SettingsPrivateGetPrefFunction::~SettingsPrivateGetPrefFunction() {}

ExtensionFunction::ResponseAction SettingsPrivateGetPrefFunction::Run() {
  return RespondNow(Error(kErrorNotSupported));
}

SettingsPrivateGetDefaultZoomFunction::
    ~SettingsPrivateGetDefaultZoomFunction() {}

ExtensionFunction::ResponseAction SettingsPrivateGetDefaultZoomFunction::Run() {
  return RespondNow(Error(kErrorNotSupported));
}

SettingsPrivateSetDefaultZoomFunction::
    ~SettingsPrivateSetDefaultZoomFunction() {}

ExtensionFunction::ResponseAction SettingsPrivateSetDefaultZoomFunction::Run() {
  return RespondNow(Error(kErrorNotSupported));
}

}  // namespace cast
}  // namespace extensions
