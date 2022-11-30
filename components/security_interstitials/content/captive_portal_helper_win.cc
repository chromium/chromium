// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/captive_portal_helper.h"

#include <netlistmgr.h>
#include <wrl/client.h>

#include "base/win/scoped_variant.h"

namespace {

bool IsNetworkBehindCaptivePortal(INetwork* network) {
  NLM_CONNECTIVITY connectivity;
  if (FAILED(network->GetConnectivity(&connectivity)))
    return false;

  if (connectivity == NLM_CONNECTIVITY_DISCONNECTED)
    return false;

  Microsoft::WRL::ComPtr<IPropertyBag> property_bag;
  if (FAILED(network->QueryInterface(IID_PPV_ARGS(&property_bag))) ||
      !property_bag) {
    return false;
  }

  base::win::ScopedVariant connectivity_variant;

  if ((connectivity & NLM_CONNECTIVITY_IPV4_SUBNET) ||
      (connectivity & NLM_CONNECTIVITY_IPV4_LOCALNETWORK) ||
      (connectivity & NLM_CONNECTIVITY_IPV4_INTERNET)) {
    // IPV4 connection:
    if (SUCCEEDED(property_bag->Read(NA_InternetConnectivityV4,
                                     connectivity_variant.Receive(),
                                     nullptr)) &&
        (V_UINT(connectivity_variant.ptr()) &
         NLM_INTERNET_CONNECTIVITY_WEBHIJACK) ==
            NLM_INTERNET_CONNECTIVITY_WEBHIJACK) {
      return true;
    }
  } else if ((connectivity & NLM_CONNECTIVITY_IPV6_SUBNET) ||
             (connectivity & NLM_CONNECTIVITY_IPV6_LOCALNETWORK) ||
             (connectivity & NLM_CONNECTIVITY_IPV6_INTERNET)) {
    // IPV6 connection:
    if (SUCCEEDED(property_bag->Read(NA_InternetConnectivityV6,
                                     connectivity_variant.Receive(),
                                     nullptr)) &&
        (V_UINT(connectivity_variant.ptr()) &
         NLM_INTERNET_CONNECTIVITY_WEBHIJACK) ==
            NLM_INTERNET_CONNECTIVITY_WEBHIJACK) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace security_interstitials {

bool IsBehindCaptivePortal() {
  // Assume the device is behind a captive portal if there is at least one
  // connected network and all connected networks are behind captive portals.
  Microsoft::WRL::ComPtr<INetworkListManager> network_list_manager;
  if (FAILED(CoCreateInstance(CLSID_NetworkListManager, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&network_list_manager)))) {
    return false;
  }

  Microsoft::WRL::ComPtr<IEnumNetworks> enum_networks;
  if (FAILED(network_list_manager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED,
                                               &enum_networks))) {
    return false;
  }

  if (!enum_networks)
    return false;

  bool found = false;
  while (true) {
    Microsoft::WRL::ComPtr<INetwork> network;
    ULONG items_returned = 0;
    // Note: MSDN documentation at
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa370740(v=vs.85).aspx
    // says items_returned is set to NULL if the first parameter is 1, but this
    // seems incorrect. In this call, items_returned is 1 until there are no
    // networks. Then it becomes zero.
    if (FAILED(enum_networks->Next(1, &network, &items_returned)))
      return false;

    if (items_returned == 0)
      break;

    if (!IsNetworkBehindCaptivePortal(network.Get()))
      return false;

    found = true;
  }
  return found;
}

}  // namespace security_interstitials
