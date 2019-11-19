// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_MESSAGES_H_
#define CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_MESSAGES_H_

#include "chrome/common/apps/platform_apps/media_galleries_permission_data.h"
#include "ipc/ipc_message_macros.h"

IPC_STRUCT_TRAITS_BEGIN(chrome_apps::MediaGalleriesPermissionData)
  IPC_STRUCT_TRAITS_MEMBER(permission())
IPC_STRUCT_TRAITS_END()

#endif  // CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_MESSAGES_H_
