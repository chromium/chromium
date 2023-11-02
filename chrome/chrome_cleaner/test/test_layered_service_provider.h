// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_LAYERED_SERVICE_PROVIDER_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_LAYERED_SERVICE_PROVIDER_H_

#include <map>

#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"

namespace chrome_cleaner {

class TestLayeredServiceProvider : public LayeredServiceProviderAPI {
 public:
  TestLayeredServiceProvider();
  ~TestLayeredServiceProvider() override;

  int EnumProtocols(int* protocols,
                    WSAPROTOCOL_INFOW* protocol_info,
                    DWORD* nb_protocol_info,
                    int* error) const override;

  int GetProviderPath(GUID* provider_id,
                      wchar_t* provider_dll_path,
                      int* provider_dll_path_len,
                      int* error) const override;

  void AddProvider(const GUID& provider_id, const base::FilePath& path);

 private:
  std::map<GUID, base::FilePath, GUIDLess> protocol_info_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_LAYERED_SERVICE_PROVIDER_H_
