// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/wifi_network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace ash {

void WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(bool is_hidden) {
  if (LoginState::IsInitialized() && LoginState::Get()->IsUserLoggedIn()) {
    base::UmaHistogramBoolean("Network.Ash.WiFi.Hidden.LoggedIn", is_hidden);
  } else {
    base::UmaHistogramBoolean("Network.Ash.WiFi.Hidden.NotLoggedIn", is_hidden);
  }
}

}  // namespace ash
