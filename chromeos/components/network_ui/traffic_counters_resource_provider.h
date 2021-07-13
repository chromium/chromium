// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_
#define CHROMEOS_COMPONENTS_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace traffic_counters {

// Adds the strings and resource paths needed for traffic counters elements
// to |html_source|.
void AddResources(content::WebUIDataSource* html_source);

}  // namespace traffic_counters
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_
