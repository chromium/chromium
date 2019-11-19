// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_

#include <string>

#include "chromecast/common/extensions_api/notifications.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace cast {
namespace api {

class NotificationsCreateFunction : public ExtensionFunction {
 protected:
  ~NotificationsCreateFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
};

class NotificationsUpdateFunction : public ExtensionFunction {
 protected:
  ~NotificationsUpdateFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.update", NOTIFICATIONS_UPDATE)
};

class NotificationsClearFunction : public ExtensionFunction {
 protected:
  ~NotificationsClearFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.clear", NOTIFICATIONS_CLEAR)
};

class NotificationsGetAllFunction : public ExtensionFunction {
 protected:
  ~NotificationsGetAllFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getAll", NOTIFICATIONS_GET_ALL)
};

class NotificationsGetPermissionLevelFunction : public ExtensionFunction {
 protected:
  ~NotificationsGetPermissionLevelFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getPermissionLevel",
                             NOTIFICATIONS_GETPERMISSIONLEVEL)
};

}  // namespace api
}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
