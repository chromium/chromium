// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/notifications/notifications_api.h"

#include "extensions/common/extension.h"

namespace extensions {
namespace cast {
namespace api {

ExtensionFunction::ResponseAction NotificationsCreateFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction NotificationsUpdateFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction NotificationsClearFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction NotificationsGetAllFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
NotificationsGetPermissionLevelFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

}  // namespace api
}  // namespace cast
}  // namespace extensions
