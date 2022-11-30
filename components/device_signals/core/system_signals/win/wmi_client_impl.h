// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_

#include "base/callback.h"
#include "components/device_signals/core/system_signals/win/wmi_client.h"

// WMI interfaces are available on Windows Vista and above, and are officially
// undocumented.
namespace device_signals {

class WmiClientImpl : public WmiClient {
 public:
  using RunWmiQueryCallback =
      base::RepeatingCallback<absl::optional<base::win::WmiError>(
          const std::wstring&,
          const std::wstring&,
          Microsoft::WRL::ComPtr<IEnumWbemClassObject>*)>;

  WmiClientImpl();

  ~WmiClientImpl() override;

  // WmiClient:
  WmiAvProductsResponse GetAntiVirusProducts() override;
  WmiHotfixesResponse GetInstalledHotfixes() override;

 private:
  friend class WmiClientImplTest;

  // Constructor taking in a `run_query_callback` which can be used to mock
  // running the WMI query.
  explicit WmiClientImpl(RunWmiQueryCallback run_query_callback);

  RunWmiQueryCallback run_query_callback_;
};

// Type shared in an internal namespace to allow for reuse in unit tests without
// duplication.
namespace internal {
// This is an undocumented structure returned from querying the "productState"
// uint32 from the AntiVirusProduct in WMI.
// http://neophob.com/2010/03/wmi-query-windows-securitycenter2/ gives a good
// summary and testing was also done with a variety of AV products to determine
// these values as accurately as possible.
#pragma pack(push)
#pragma pack(1)
struct PRODUCT_STATE {
  uint8_t unknown_1 : 4;
  uint8_t definition_state : 4;  // 1 = Out of date, 0 = Up to date.
  uint8_t unknown_2 : 4;
  uint8_t security_state : 4;  //  0 = Inactive, 1 = Active, 2 = Snoozed.
  uint8_t security_provider;   // matches WSC_SECURITY_PROVIDER in wscapi.h.
  uint8_t unknown_3;
};
#pragma pack(pop)
}  // namespace internal

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_
