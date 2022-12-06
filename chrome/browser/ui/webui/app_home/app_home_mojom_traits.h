// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace mojo {

template <>
struct EnumTraits<app_home::mojom::RunOnOsLoginMode,
                  web_app::RunOnOsLoginMode> {
  static app_home::mojom::RunOnOsLoginMode ToMojom(
      web_app::RunOnOsLoginMode input);
  static bool FromMojom(app_home::mojom::RunOnOsLoginMode input,
                        web_app::RunOnOsLoginMode* output);
};
}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_MOJOM_TRAITS_H_
