// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/traffic_counters_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"

namespace ash {

namespace {

// Interval duration to determine the auto reset check frequency.
constexpr base::TimeDelta kResetCheckInterval = base::Hours(6);

// Default reset day assigned to a network when the reset day has not been set.
constexpr int kDefaultResetDay = 1;

// Stores the number of bytes per MB for UMA purposes.
constexpr uint64_t kBytesPerMb = 1024 * 1024;

// Holds an unowned pointer the Singleton instance of this class.
traffic_counters::TrafficCountersHandler* g_traffic_counters_handler = nullptr;

// Gets the last day of the month when the user specified reset day
// exceeds the number of day in that month. For example, if the user specified
// reset day is 31, this function would ensure that the returned time would
// represent Feb 28 (non-leap year), Apr 30, etc.
base::Time GetValidTime(base::Time::Exploded exploded_time) {
  base::Time time;
  while (!base::Time::FromLocalExploded(exploded_time, &time)) {
    if (exploded_time.day_of_month > 28) {
      --exploded_time.day_of_month;
    } else {
      break;
    }
  }
  return time;
}

// Calculates when the traffic counters were expected to be reset last month.
base::Time CalculateLastMonthResetTime(base::Time::Exploded exploded,
                                       int user_specified_reset_day) {
  exploded.month -= 1;
  if (exploded.month < 1) {
    exploded.month = 12;
    exploded.year -= 1;
  }

  exploded.day_of_month = user_specified_reset_day;

  return GetValidTime(exploded);
}

// Calculates when the traffic counters were/are expected to be reset this
// month.
base::Time CalculateCurrentMonthResetTime(base::Time::Exploded exploded,
                                          int user_specified_reset_day) {
  exploded.day_of_month = user_specified_reset_day;

  return GetValidTime(exploded);
}

// To avoid discrepancies between different times of the same day, set all times
// to 12:00:00 AM. This is safe to do so because traffic counters will never be
// automatically reset more than once on any given day.
void AdjustExplodedTimeValues(base::Time::Exploded* exploded_time) {
  exploded_time->hour = 0;
  exploded_time->minute = 0;
  exploded_time->second = 0;
  exploded_time->millisecond = 0;
}

// Returns a string representing of the network technology type.
std::string GetNetworkTechnologyString(
    NetworkState::NetworkTechnologyType type) {
  switch (type) {
    case NetworkState::NetworkTechnologyType::kCellular:
      return "Cellular";
    case NetworkState::NetworkTechnologyType::kEthernet:
      return "Ethernet";
    case NetworkState::NetworkTechnologyType::kEthernetEap:
      return "EthernetEap";
    case NetworkState::NetworkTechnologyType::kWiFi:
      return "WiFi";
    case NetworkState::NetworkTechnologyType::kTether:
      return "Tether";
    case NetworkState::NetworkTechnologyType::kVPN:
      return "VPN";
    case NetworkState::NetworkTechnologyType::kUnknown:
      return "Unknown";
  }
}

// Since rx_bytes and tx_bytes may be larger than the maximum value
// representable by uint32_t, we must check whether it was implicitly converted
// to a double during D-Bus deserialization.
uint64_t GetBytes(const base::Value::Dict& tc_dict, const std::string& key) {
  uint64_t bytes = 0;
  if (const base::Value* const value = tc_dict.Find(key)) {
    if (value->is_int()) {
      bytes = value->GetInt();
    } else if (value->is_double()) {
      bytes = std::floor(value->GetDouble());
    } else {
      NET_LOG(ERROR) << "Unexpected type " << value->type() << " for " << key;
    }
  } else {
    NET_LOG(ERROR) << "Missing field: " << key;
  }
  return bytes;
}

// Checks whether traffic counters operations (like ResetTrafficCounters) are
// allowed for the network specified by |service_path|.
bool AreTrafficCounterOperationsAllowed(const std::string& service_path) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);

  if (!network_state) {
    NET_LOG(ERROR) << "AreTrafficCounterOperationsAllowed for network "
                   << NetworkPathId(service_path)
                   << " failed: network state is null";
    return false;
  }

  auto network_technology = network_state->GetNetworkTechnologyType();

  if (network_technology != NetworkState::NetworkTechnologyType::kCellular &&
      network_technology != NetworkState::NetworkTechnologyType::kWiFi) {
    NET_LOG(ERROR) << "AreTrafficCounterOperationsAllowed for network "
                   << NetworkPathId(service_path)
                   << " failed: unexpected network technology type: "
                   << GetNetworkTechnologyString(network_technology);
    return false;
  }

  if (network_technology == NetworkState::NetworkTechnologyType::kWiFi &&
      !features::IsTrafficCountersForWiFiTestingEnabled()) {
    NET_LOG(ERROR) << "AreTrafficCounterOperationsAllowed for network "
                   << NetworkPathId(service_path)
                   << " failed: traffic_counters for WiFi disabled";
    return false;
  }

  return true;
}

}  // namespace

namespace traffic_counters {

// static
void TrafficCountersHandler::Initialize() {
  CHECK(!g_traffic_counters_handler);
  g_traffic_counters_handler = new TrafficCountersHandler();
  g_traffic_counters_handler->StartAutoReset();
}

// static
void TrafficCountersHandler::Shutdown() {
  CHECK(g_traffic_counters_handler);
  delete g_traffic_counters_handler;
  g_traffic_counters_handler = nullptr;
}

// static
TrafficCountersHandler* TrafficCountersHandler::Get() {
  CHECK(g_traffic_counters_handler)
      << "TrafficCountersHandler::Get() called before Initialize()";
  return g_traffic_counters_handler;
}

// static
void TrafficCountersHandler::InitializeForTesting() {
  CHECK(!g_traffic_counters_handler);
  // Note that unlike Initialize(), this function does not call
  // StartAutoReset(). This allows test properties to be set before the test
  // runs. Call StartAutoResetForTesting() after the test properties have been
  // set.
  g_traffic_counters_handler = new TrafficCountersHandler();
}

// static
void TrafficCountersHandler::StartAutoResetForTesting() {
  CHECK(g_traffic_counters_handler)
      << "TrafficCountersHandler::StartAutoResetForTesting() called before "
         "Initialize()";
  StartAutoReset();
}

// static
bool TrafficCountersHandler::IsInitialized() {
  return g_traffic_counters_handler != nullptr;
}

TrafficCountersHandler::TrafficCountersHandler()
    : time_getter_(base::BindRepeating([]() { return base::Time::Now(); })),
      timer_(std::make_unique<base::RepeatingTimer>()) {
  CHECK(features::IsTrafficCountersEnabled());
}

TrafficCountersHandler::~TrafficCountersHandler() = default;

void TrafficCountersHandler::StartAutoReset() {
  RunAutoResetTrafficCountersForActiveNetworks();
  timer_->Start(
      FROM_HERE, kResetCheckInterval, this,
      &TrafficCountersHandler::RunAutoResetTrafficCountersForActiveNetworks);
}

void TrafficCountersHandler::RequestTrafficCounters(
    const std::string& service_path,
    RequestTrafficCountersCallback callback) {
  if (!AreTrafficCounterOperationsAllowed(service_path)) {
    NET_LOG(ERROR) << "RequestTrafficCounters for network "
                   << NetworkPathId(service_path) << " failed";
    std::move(callback).Run(std::nullopt);
    return;
  }
  NetworkHandler::Get()->network_state_handler()->RequestTrafficCounters(
      service_path, std::move(callback));
}

void TrafficCountersHandler::ResetTrafficCounters(
    const std::string& service_path) {
  if (!AreTrafficCounterOperationsAllowed(service_path)) {
    NET_LOG(ERROR) << "ResetTrafficCounters for network "
                   << NetworkPathId(service_path) << " failed";
    return;
  }
  RequestTrafficCounters(
      service_path,
      base::BindOnce(
          &TrafficCountersHandler::
              OnTrafficCountersRequestedForTrackingDailyAverageDataUsage,
          weak_ptr_factory_.GetWeakPtr(), service_path));
}

void TrafficCountersHandler::
    OnTrafficCountersRequestedForTrackingDailyAverageDataUsage(
        const std::string& service_path,
        std::optional<base::Value> traffic_counters) {
  double total_data_usage = 0;
  if (!traffic_counters || !traffic_counters->is_list() ||
      !traffic_counters->GetList().size()) {
    NET_LOG(ERROR) << "Failed to get traffic counters for tracking daily "
                   << "average for network:" << NetworkPathId(service_path);
    NetworkHandler::Get()
        ->managed_network_configuration_handler()
        ->GetManagedProperties(
            LoginState::Get()->primary_user_hash(), service_path,
            base::BindOnce(
                &TrafficCountersHandler::OnGetManagedPropertiesForLastResetTime,
                weak_ptr_factory_.GetWeakPtr(), /*total_data_usage=*/0.0));
    return;
  }
  for (const base::Value& tc : traffic_counters->GetList()) {
    DCHECK(tc.is_dict());

    const base::Value::Dict& tc_dict = tc.GetDict();
    total_data_usage += GetBytes(tc_dict, "rx_bytes");
    total_data_usage += GetBytes(tc_dict, "tx_bytes");
  }
  NET_LOG(EVENT) << "Total data usage for network "
                 << NetworkPathId(service_path) << ": " << total_data_usage;
  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->GetManagedProperties(
          LoginState::Get()->primary_user_hash(), service_path,
          base::BindOnce(
              &TrafficCountersHandler::OnGetManagedPropertiesForLastResetTime,
              weak_ptr_factory_.GetWeakPtr(), total_data_usage));
}

void TrafficCountersHandler::OnGetManagedPropertiesForLastResetTime(
    double total_data_usage,
    const std::string& service_path,
    std::optional<base::Value::Dict> properties,
    std::optional<std::string> error) {
  // Since last reset time has already been retrieved (via
  // GetManagedProperties), the network's traffic counters can be reset and the
  // last reset time can be modified in the platform.
  NetworkHandler::Get()->network_state_handler()->ResetTrafficCounters(
      service_path);
  if (!properties) {
    NET_LOG(ERROR) << "GetManagedProperties failed for: "
                   << NetworkPathId(service_path)
                   << " Error: " << error.value_or("Failed");
    return;
  }
  const std::optional<double> last_reset =
      properties->FindDouble(::onc::network_config::kTrafficCounterResetTime);
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network_state == nullptr) {
    NET_LOG(ERROR) << "Failed to retrieve NetworkState (and Technology type) "
                      "during ResetTrafficCounters for "
                   << NetworkPathId(service_path);
    return;
  }
  const std::string network_technology =
      GetNetworkTechnologyString(network_state->GetNetworkTechnologyType());
  if (!last_reset.has_value()) {
    // Any network that does not have a last reset time was reset during the
    // first TrafficCountersHandler run. So, a failure to find the last reset
    // time of this network is unexpected.
    NET_LOG(ERROR) << "Failed to retrieve last rest time for network: "
                   << NetworkPathId(service_path);

    base::UmaHistogramMemoryLargeMB("Network.TrafficCounters." +
                                        network_technology +
                                        ".DataUsageNoLastResetTime",
                                    (total_data_usage / kBytesPerMb));
    return;
  }

  base::Time last_reset_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(last_reset.value()));
  base::Time current_time = time_getter_.Run();
  int total_days_since_last_reset =
      (current_time - last_reset_time).InDays() + 1;
  base::UmaHistogramMemoryLargeMB(
      "Network.TrafficCounters." + network_technology +
          ".AverageDailyDataUsage",
      (total_data_usage / kBytesPerMb) / total_days_since_last_reset);
}

void TrafficCountersHandler::SetTrafficCountersResetDay(
    const std::string& guid,
    uint32_t day,
    SetTrafficCountersResetDayCallback callback) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  CHECK(network_state);
  if (!AreTrafficCounterOperationsAllowed(network_state->path())) {
    NET_LOG(ERROR) << "SetTrafficCountersResetDay for network "
                   << NetworkGuidId(guid) << " failed";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (day < 1 || day > 31) {
    NET_LOG(ERROR) << "Failed to set reset day " << day << " for "
                   << NetworkGuidId(guid)
                   << ": day must be between 1 and 31 (inclusive)";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetDayOfTrafficCountersAutoReset(guid, day);
  std::move(callback).Run(/*success=*/true);
}

uint32_t TrafficCountersHandler::GetUserSpecifiedResetDay(
    const std::string& guid) {
  const base::Value* reset_day_ptr =
      NetworkHandler::Get()
          ->network_metadata_store()
          ->GetDayOfTrafficCountersAutoReset(guid);
  if (!reset_day_ptr) {
    NET_LOG(ERROR) << "Failed to get auto reset day for network: "
                   << NetworkGuidId(guid);
    return kDefaultResetDay;
  }
  CHECK(reset_day_ptr) << "Reset day found for guid: " << NetworkGuidId(guid)
                       << ", value: " << reset_day_ptr->GetInt();

  return reset_day_ptr->GetInt();
}

void TrafficCountersHandler::RunAutoResetTrafficCountersForActiveNetworks() {
  NET_LOG(EVENT)
      << "Starting RunAutoResetTrafficCountersForActiveNetworks() at: "
      << time_getter_.Run();
  // Retrieve list of currently active networks.
  NetworkStateHandler::NetworkStateList active_network_states;
  auto network_type_pattern = NetworkTypePattern::Cellular();
  if (features::IsTrafficCountersForWiFiTestingEnabled()) {
    network_type_pattern = network_type_pattern | NetworkTypePattern::WiFi();
  }
  NetworkHandler::Get()->network_state_handler()->GetActiveNetworkListByType(
      network_type_pattern, &active_network_states);
  NET_LOG(EVENT) << "TrafficCountersHandler found "
                 << active_network_states.size() << " active networks";
  for (const auto& network : active_network_states) {
    NET_LOG(EVENT) << "Retrieving managed network configuration properties for "
                      "network "
                   << NetworkGuidId(network->guid());
    const std::string service_path = network->path();
    NetworkHandler::Get()
        ->managed_network_configuration_handler()
        ->GetManagedProperties(
            LoginState::Get()->primary_user_hash(), service_path,
            base::BindOnce(
                &TrafficCountersHandler::OnGetManagedPropertiesForAutoReset,
                weak_ptr_factory_.GetWeakPtr(), network->guid()));
  }
}

void TrafficCountersHandler::OnGetManagedPropertiesForAutoReset(
    std::string guid,
    const std::string& service_path,
    std::optional<base::Value::Dict> properties,
    std::optional<std::string> error) {
  if (!properties) {
    NET_LOG(ERROR) << "GetManagedProperties failed for: " << NetworkGuidId(guid)
                   << " Error: " << error.value_or("Failed");
    return;
  }
  const std::optional<double> last_reset =
      properties->FindDouble(::onc::network_config::kTrafficCounterResetTime);
  // The last reset time for a network should only be unset once
  // during the first traffic counters run for a "new" network.
  if (!last_reset.has_value()) {
    // No last reset time, trigger an initial reset.
    NET_LOG(EVENT) << "Resetting traffic counters for network: "
                   << NetworkGuidId(guid);
    ResetTrafficCounters(service_path);
    return;
  }

  const base::Value* reset_day_ptr =
      NetworkHandler::Get()
          ->network_metadata_store()
          ->GetDayOfTrafficCountersAutoReset(guid);
  // The user specified reset day for a network should only be unset once
  // during the first traffic counters run for a "new" network.
  if (!reset_day_ptr) {
    NET_LOG(EVENT) << "Failed to retrieve auto reset day for network "
                   << NetworkGuidId(guid) << ", setting auto reset day to "
                   << kDefaultResetDay << " and resetting traffic counters "
                   << "for the network";
    ResetTrafficCounters(service_path);
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetDayOfTrafficCountersAutoReset(guid, kDefaultResetDay);
    return;
  }

  auto user_specified_reset_day = reset_day_ptr->GetInt();
  NET_LOG(EVENT) << "The user specified reset day for network "
                 << NetworkGuidId(guid) << " is " << user_specified_reset_day;

  base::Time::Exploded current_time_exploded = CurrentDateExploded();

  base::Time last_month_reset = CalculateLastMonthResetTime(
      current_time_exploded, user_specified_reset_day);
  base::Time last_reset_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(last_reset.value()));
  // If the last time traffic counters were reset (last_reset_time) was before
  // the time traffic counters were expected to be reset last month
  // (last_month_reset), then the traffic counters should be reset. This handles
  // the case where the traffic counters feature was disabled for over a month
  // and then re-enabled.
  if (last_reset_time < last_month_reset) {
    NET_LOG(EVENT) << "Resetting traffic counters since " << last_reset_time
                   << " < " << last_month_reset;
    ResetTrafficCounters(service_path);
    return;
  }

  base::Time curr_month_reset = CalculateCurrentMonthResetTime(
      current_time_exploded, user_specified_reset_day);
  base::Time current_date = GetValidTime(current_time_exploded);
  // If the last time traffic counters were reset (last_reset_time) was before
  // the expected reset date on this month (curr_month_resset), and today
  // (current_date) is equal to or greater than the expected date of reset for
  // this month (curr_month_reset), then reset the counters. For example, let's
  // assume that traffic counters are to be reset on the 5th of every month and
  // were last reset on January 5th. If the current_date is between February
  // 1st-February 4th, then current_date (e.g, Feb 3rd) < curr_month_reset (Feb
  // 5th), so the counters are not reset. However, if the current date is Feb
  // 5th onwards, then current_date (Feb 5th) = curr_month_reset (Feb 5th), so
  // traffic counters are reset.
  if (last_reset_time < curr_month_reset && current_date >= curr_month_reset) {
    NET_LOG(EVENT) << "Resetting traffic counters since " << last_reset_time
                   << " < " << curr_month_reset << " && " << current_date
                   << " >= " << curr_month_reset;
    ResetTrafficCounters(service_path);
  }
}

base::Time::Exploded TrafficCountersHandler::CurrentDateExploded() {
  base::Time::Exploded current_time_exploded;
  time_getter_.Run().LocalExplode(&current_time_exploded);
  AdjustExplodedTimeValues(&current_time_exploded);

  return current_time_exploded;
}

void TrafficCountersHandler::RunForTesting() {
  RunAutoResetTrafficCountersForActiveNetworks();
}

void TrafficCountersHandler::SetTimeGetterForTesting(TimeGetter time_getter) {
  time_getter_ = std::move(time_getter);
}

}  // namespace traffic_counters

}  // namespace ash
