// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_

#include "chromeos/crosapi/mojom/web_app_types.mojom.h"

namespace webapps {
enum class InstallResultCode;
enum class UninstallResultCode;
}

namespace mojo {

template <>
struct EnumTraits<crosapi::mojom::WebAppInstallResultCode,
                  webapps::InstallResultCode> {
  static crosapi::mojom::WebAppInstallResultCode ToMojom(
      webapps::InstallResultCode input);
  static bool FromMojom(crosapi::mojom::WebAppInstallResultCode input,
                        webapps::InstallResultCode* output);
};

template <>
struct EnumTraits<crosapi::mojom::WebAppUninstallResultCode,
                  webapps::UninstallResultCode> {
  static crosapi::mojom::WebAppUninstallResultCode ToMojom(
      webapps::UninstallResultCode input);
  static bool FromMojom(crosapi::mojom::WebAppUninstallResultCode input,
                        webapps::UninstallResultCode* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_
