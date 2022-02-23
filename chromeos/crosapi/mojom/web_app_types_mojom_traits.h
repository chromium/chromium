// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_

#include "chromeos/crosapi/mojom/web_app_types.mojom.h"

namespace webapps {
enum class InstallResultCode;
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

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_WEB_APP_TYPES_MOJOM_TRAITS_H_
