// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_LAYERED_SERVICE_PROVIDER_WRAPPER_H_
#define CHROME_CHROME_CLEANER_OS_LAYERED_SERVICE_PROVIDER_WRAPPER_H_

#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"

namespace chrome_cleaner {

// This class is used as a wrapper around the OS calls to interact with layered
// service providers (aka LSPs). This allows test to more easily provide mock
// for these OS calls.
class LayeredServiceProviderWrapper : public LayeredServiceProviderAPI {
 public:
  int EnumProtocols(int* protocols,
                    WSAPROTOCOL_INFOW* protocol_info,
                    DWORD* nb_protocol_info,
                    int* error) const override;
  int GetProviderPath(GUID* provider_id,
                      wchar_t* provider_dll_path,
                      int* provider_dll_path_len,
                      int* error) const override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_LAYERED_SERVICE_PROVIDER_WRAPPER_H_
