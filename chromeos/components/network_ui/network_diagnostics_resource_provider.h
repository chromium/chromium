// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_
#define CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace network_diagnostics {

// Adds the strings and resource paths needed for network diagnostics elements
// to |html_source|.
void AddResources(content::WebUIDataSource* html_source);

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_
