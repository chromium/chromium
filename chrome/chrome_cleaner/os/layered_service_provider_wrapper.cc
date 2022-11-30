// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"

#include <ws2spi.h>

namespace chrome_cleaner {

int LayeredServiceProviderWrapper::EnumProtocols(
    int* protocols,
    WSAPROTOCOL_INFOW* protocol_info,
    DWORD* nb_protocol_info,
    int* error) const {
  return ::WSCEnumProtocols(protocols, protocol_info, nb_protocol_info, error);
}

int LayeredServiceProviderWrapper::GetProviderPath(GUID* provider_id,
                                                   wchar_t* provider_dll_path,
                                                   int* provider_dll_path_len,
                                                   int* error) const {
  return ::WSCGetProviderPath(provider_id, provider_dll_path,
                              provider_dll_path_len, error);
}

}  // namespace chrome_cleaner
