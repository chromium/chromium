// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_mojom_traits.h"

namespace mojo {

app_home::mojom::RunOnOsLoginMode EnumTraits<
    app_home::mojom::RunOnOsLoginMode,
    web_app::RunOnOsLoginMode>::ToMojom(web_app::RunOnOsLoginMode input) {
  switch (input) {
    case web_app::RunOnOsLoginMode::kNotRun:
      return app_home::mojom::RunOnOsLoginMode::kNotRun;
    case web_app::RunOnOsLoginMode::kWindowed:
      return app_home::mojom::RunOnOsLoginMode::kWindowed;
    case web_app::RunOnOsLoginMode::kMinimized:
      return app_home::mojom::RunOnOsLoginMode::kMinimized;
  }
}

bool EnumTraits<app_home::mojom::RunOnOsLoginMode, web_app::RunOnOsLoginMode>::
    FromMojom(app_home::mojom::RunOnOsLoginMode input,
              web_app::RunOnOsLoginMode* output) {
  switch (input) {
    case app_home::mojom::RunOnOsLoginMode::kNotRun:
      *output = web_app::RunOnOsLoginMode::kNotRun;
      return true;
    case app_home::mojom::RunOnOsLoginMode::kWindowed:
      *output = web_app::RunOnOsLoginMode::kWindowed;
      return true;
    case app_home::mojom::RunOnOsLoginMode::kMinimized:
      *output = web_app::RunOnOsLoginMode::kMinimized;
      return true;
  }
}
}  // namespace mojo
