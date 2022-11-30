// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_layered_service_provider.h"

namespace chrome_cleaner {

TestLayeredServiceProvider::TestLayeredServiceProvider() = default;

TestLayeredServiceProvider::~TestLayeredServiceProvider() = default;

int TestLayeredServiceProvider::EnumProtocols(int* protocols,
                                              WSAPROTOCOL_INFOW* protocol_info,
                                              DWORD* nb_protocol_info,
                                              int* error) const {
  // We don't use the protocols argument yet.
  DCHECK(!protocols);
  DCHECK(nb_protocol_info);
  DCHECK(error);
  size_t bytes_needed = protocol_info_.size() * sizeof(WSAPROTOCOL_INFOW);
  if (*nb_protocol_info < bytes_needed) {
    *nb_protocol_info = bytes_needed;
    *error = WSAENOBUFS;
    return SOCKET_ERROR;
  }
  *nb_protocol_info = bytes_needed;
  std::map<GUID, base::FilePath, GUIDLess>::const_iterator iter =
      protocol_info_.begin();
  for (size_t i = 0; iter != protocol_info_.end(); ++iter, ++i) {
    protocol_info[i].ProviderId = iter->first;
  }
  return protocol_info_.size();
}

int TestLayeredServiceProvider::GetProviderPath(GUID* provider_id,
                                                wchar_t* provider_dll_path,
                                                int* provider_dll_path_len,
                                                int* error) const {
  DCHECK(provider_id);
  DCHECK(provider_dll_path);
  DCHECK(provider_dll_path_len);
  DCHECK(error);
  std::map<GUID, base::FilePath, GUIDLess>::const_iterator iter =
      protocol_info_.find(*provider_id);
  if (iter == protocol_info_.end()) {
    *error = WSAEINVAL;
    return SOCKET_ERROR;
  }
  if (*provider_dll_path_len <
      static_cast<int>(iter->second.value().size() + 1)) {
    *error = WSAEFAULT;
    return SOCKET_ERROR;
  }
  ::wcsncpy(provider_dll_path, iter->second.value().c_str(),
            iter->second.value().size());
  provider_dll_path[iter->second.value().size()] = L'\0';
  return 0;
}

void TestLayeredServiceProvider::AddProvider(const GUID& provider_id,
                                             const base::FilePath& path) {
  protocol_info_[provider_id] = path;
}

}  // namespace chrome_cleaner
