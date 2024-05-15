// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/statistics_provider.h"

#include <array>
#include <string_view>

#include "base/memory/singleton.h"
#include "chromeos/ash/components/system/statistics_provider_impl.h"

namespace ash::system {

namespace {
// These are the machine serial number keys that we check in order until we find
// a non-empty serial number.
//
// On older Samsung devices the VPD contains two serial numbers: "Product_S/N"
// and "serial_number" which are based on the same value except that the latter
// has a letter appended that serves as a check digit. Unfortunately, the
// sticker on the device packaging didn't include that check digit (the sticker
// on the device did though!). The former sticker was the source of the serial
// number used by device management service, so we preferred "Product_S/N" over
// "serial_number" to match the server. As an unintended consequence, older
// Samsung devices display and report a serial number that doesn't match the
// sticker on the device (the check digit is missing).
//
// "Product_S/N" is known to be used on celes, lumpy, pi, pit, snow, winky and
// some kevin devices and thus needs to be supported until AUE of these
// devices. It's known *not* to be present on caroline.
// TODO(tnagel): Remove "Product_S/N" after all devices that have it are AUE.
constexpr std::array kMachineInfoSerialNumberKeys = {
    kSerialNumberKey,        // VPD v2+ devices (Samsung: caroline and later)
    kFlexIdKey,              // Used by Reven devices
    kLegacySerialNumberKey,  // Samsung legacy
};
}  // namespace

// The StatisticsProvider implementation used in production.
class StatisticsProviderSingleton final : public StatisticsProviderImpl {
 public:
  static StatisticsProviderSingleton* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<StatisticsProviderSingleton>;

  StatisticsProviderSingleton() = default;
  ~StatisticsProviderSingleton() override = default;
};

// static
StatisticsProviderSingleton* StatisticsProviderSingleton::GetInstance() {
  return base::Singleton<
      StatisticsProviderSingleton,
      base::DefaultSingletonTraits<StatisticsProviderSingleton>>::get();
}

// static
bool StatisticsProvider::FlagValueToBool(FlagValue value, bool default_value) {
  switch (value) {
    case FlagValue::kUnset:
      return default_value;
    case FlagValue::kTrue:
      return true;
    case FlagValue::kFalse:
      return false;
  }
}

std::optional<std::string_view> StatisticsProvider::GetMachineID() {
  for (const char* key : kMachineInfoSerialNumberKeys) {
    auto machine_id = GetMachineStatistic(key);
    if (machine_id && !machine_id->empty()) {
      return machine_id.value();
    }
  }
  return std::nullopt;
}

static StatisticsProvider* g_test_statistics_provider = nullptr;

// static
StatisticsProvider* StatisticsProvider::GetInstance() {
  if (g_test_statistics_provider)
    return g_test_statistics_provider;
  return StatisticsProviderSingleton::GetInstance();
}

// static
void StatisticsProvider::SetTestProvider(StatisticsProvider* test_provider) {
  g_test_statistics_provider = test_provider;
}

}  // namespace ash::system
