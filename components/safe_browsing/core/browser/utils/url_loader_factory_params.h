// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_URL_LOADER_FACTORY_PARAMS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_URL_LOADER_FACTORY_PARAMS_H_

#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace safe_browsing {

// Get the params used for Safe Browsing URLLoaderFactory construction.
network::mojom::URLLoaderFactoryParamsPtr GetUrlLoaderFactoryParams();

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_URL_LOADER_FACTORY_PARAMS_H_
