// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hidden_network_handler.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {
namespace {

constexpr char kRemoveAttemptResultHistogram[] =
    "Network.Ash.WiFi.Hidden.RemovalAttempt.Result";

constexpr base::TimeDelta kDefaultOverrideInterval = base::Minutes(1);
constexpr base::TimeDelta kOneDay = base::Days(1);
constexpr base::TimeDelta kInitialDelay = base::Seconds(30);

void OnRemoveConfigurationSuccess(const std::string guid) {
  base::UmaHistogramBoolean(kRemoveAttemptResultHistogram, true);
  NET_LOG(EVENT) << "Successfully removed wrongly hidden network: " << guid;
}

void OnRemoveConfigurationFailure(const std::string guid,
                                  const std::string& error_name) {
  base::UmaHistogramBoolean(kRemoveAttemptResultHistogram, false);
  NET_LOG(EVENT) << "Failed to remove wrongly hidden network: " << guid
                 << ", error: " << error_name;
}

base::TimeDelta ComputeMigrationInterval() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kHiddenNetworkMigrationInterval)) {
    return kOneDay;
  }

  int interval_in_seconds = -1;
  const std::string ascii = command_line->GetSwitchValueASCII(
      switches::kHiddenNetworkMigrationInterval);

  if (ascii.empty() || !base::StringToInt(ascii, &interval_in_seconds) ||
      interval_in_seconds < 1) {
    return kDefaultOverrideInterval;
  }
  return base::Seconds(interval_in_seconds);
}

}  // namespace

void HiddenNetworkHandler::Init(
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkStateHandler* network_state_handler) {
  DCHECK(NetworkHandler::IsInitialized());
  network_state_handler_ = network_state_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
}

void HiddenNetworkHandler::SetNetworkMetadataStore(
    NetworkMetadataStore* network_metadata_store) {
  if (network_metadata_store_) {
    initial_delay_timer_.Stop();
    daily_event_timer_.Stop();
  }

  network_metadata_store_ = network_metadata_store;
  if (!network_metadata_store_) {
    return;
  }

  initial_delay_timer_.Start(
      FROM_HERE, kInitialDelay,
      base::BindOnce(&HiddenNetworkHandler::CleanHiddenNetworks,
                     base::Unretained(this)));
  daily_event_timer_.Start(
      FROM_HERE, ComputeMigrationInterval(),
      base::BindRepeating(&HiddenNetworkHandler::CleanHiddenNetworks,
                          base::Unretained(this)));
}

void HiddenNetworkHandler::CleanHiddenNetworks() {
  NetworkStateHandler::NetworkStateList state_list;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::WiFi(),
                                               /*configured_only=*/true,
                                               /*visible_only=*/false,
                                               /*limit=*/0, &state_list);
  size_t remove_network_attempts = 0;

  for (const NetworkState* state : state_list) {
    if (!state->hidden_ssid() || state->IsManagedByPolicy()) {
      continue;
    }

    // The last connected timestamp for a network will be zero if the network
    // has never been connected to.
    if (!network_metadata_store_->GetLastConnectedTimestamp(state->guid())
             .is_zero()) {
      continue;
    }

    // The WiFi timestamp will return UnixEpoch() if the network has
    // existed for more than two weeks.
    if (network_metadata_store_->UpdateAndRetrieveWiFiTimestamp(
            state->guid()) != base::Time::UnixEpoch()) {
      continue;
    }

    NET_LOG(EVENT) << "Attempting to remove network configuration with GUID: "
                   << state->guid();

    managed_network_configuration_handler_->RemoveConfiguration(
        state->path(),
        base::BindOnce(&OnRemoveConfigurationSuccess, state->guid()),
        base::BindOnce(&OnRemoveConfigurationFailure, state->guid()));

    remove_network_attempts++;
  }

  base::UmaHistogramCounts100("Network.Ash.WiFi.Hidden.RemovalAttempt",
                              remove_network_attempts);
}

}  // namespace ash
