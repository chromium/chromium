// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_

#include "base/functional/callback.h"
#include "components/device_signals/core/system_signals/win/wmi_client.h"

// WMI interfaces are available on Windows Vista and above, and are officially
// undocumented.
namespace device_signals {

class WmiClientImpl : public WmiClient {
 public:
  using RunWmiQueryCallback =
      base::RepeatingCallback<std::optional<base::win::WmiError>(
          const std::wstring&,
          const std::wstring&,
          Microsoft::WRL::ComPtr<IEnumWbemClassObject>*)>;

  WmiClientImpl();

  ~WmiClientImpl() override;

  // WmiClient:
  WmiHotfixesResponse GetInstalledHotfixes() override;

 private:
  friend class WmiClientImplTest;

  // Constructor taking in a `run_query_callback` which can be used to mock
  // running the WMI query.
  explicit WmiClientImpl(RunWmiQueryCallback run_query_callback);

  RunWmiQueryCallback run_query_callback_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WMI_CLIENT_IMPL_H_
