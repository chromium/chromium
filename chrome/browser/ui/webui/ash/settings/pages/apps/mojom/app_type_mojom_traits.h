// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_MOJOM_APP_TYPE_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_MOJOM_APP_TYPE_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_notification_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"

namespace mojo {

namespace {

using Readiness = ash::settings::app_notification::mojom::Readiness;

}  // namespace

template <>
struct EnumTraits<Readiness, apps::Readiness> {
  static Readiness ToMojom(apps::Readiness input);
  static bool FromMojom(Readiness input, apps::Readiness* output);
};

template <>
struct CloneTraits<apps::PermissionPtr> {
  static apps::PermissionPtr Clone(const apps::PermissionPtr& input) {
    return input->Clone();
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_MOJOM_APP_TYPE_MOJOM_TRAITS_H_
