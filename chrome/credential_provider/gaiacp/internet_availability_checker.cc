// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"

#include <netlistmgr.h>  // For CLSID_NetworkListManager
#include <wrl/client.h>

#include "base/win/atl.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

namespace {

bool InternetConnectionAvailable() {
  // If any errors occur, return that internet connection is available.  At
  // worst the credential provider will try to connect and fail.

  Microsoft::WRL::ComPtr<INetworkListManager> manager;
  HRESULT hr = ::CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&manager));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CoCreateInstance(NetworkListManager) hr=" << putHR(hr);
    return true;
  }

  VARIANT_BOOL is_connected;
  hr = manager->get_IsConnectedToInternet(&is_connected);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "manager->get_IsConnectedToInternet hr=" << putHR(hr);
    return true;
  }

  // Normally VARIANT_TRUE/VARIANT_FALSE are used with the type VARIANT_BOOL
  // but in this case the docs explicitly say to use FALSE.
  // https://docs.microsoft.com/en-us/windows/desktop/api/Netlistmgr/
  //     nf-netlistmgr-inetworklistmanager-get_isconnectedtointernet
  return is_connected != FALSE;
}

}  // namespace

// static
InternetAvailabilityChecker* InternetAvailabilityChecker::Get() {
  return *GetInstanceStorage();
}

// static
void InternetAvailabilityChecker::SetInstanceForTesting(
    InternetAvailabilityChecker* instance) {
  *GetInstanceStorage() = instance;
}

// static
InternetAvailabilityChecker**
InternetAvailabilityChecker::GetInstanceStorage() {
  static InternetAvailabilityChecker instance;
  static InternetAvailabilityChecker* instance_storage = &instance;

  return &instance_storage;
}

InternetAvailabilityChecker::InternetAvailabilityChecker() = default;

InternetAvailabilityChecker::~InternetAvailabilityChecker() = default;

bool InternetAvailabilityChecker::HasInternetConnection() {
  return InternetConnectionAvailable();
}

}  // namespace credential_provider
