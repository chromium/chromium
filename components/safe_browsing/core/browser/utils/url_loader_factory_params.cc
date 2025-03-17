// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/utils/url_loader_factory_params.h"

namespace safe_browsing {

network::mojom::URLLoaderFactoryParamsPtr GetUrlLoaderFactoryParams() {
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_orb_enabled = false;
  params->is_trusted = true;
  return params;
}

}  // namespace safe_browsing
