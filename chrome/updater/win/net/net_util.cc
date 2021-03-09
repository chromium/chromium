// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/net_util.h"

#include <vector>

namespace updater {

HRESULT QueryHeadersString(HINTERNET request_handle,
                           uint32_t info_level,
                           const wchar_t* name,
                           std::wstring* value) {
  DWORD num_bytes = 0;
  ::WinHttpQueryHeaders(request_handle, info_level, name,
                        WINHTTP_NO_OUTPUT_BUFFER, &num_bytes,
                        WINHTTP_NO_HEADER_INDEX);
  auto hr = HRESULTFromLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
    return hr;
  std::vector<wchar_t> buffer(num_bytes / sizeof(wchar_t));
  if (!::WinHttpQueryHeaders(request_handle, info_level, name, &buffer.front(),
                             &num_bytes, WINHTTP_NO_HEADER_INDEX)) {
    return HRESULTFromLastError();
  }
  DCHECK_EQ(0u, num_bytes % sizeof(wchar_t));
  buffer.resize(num_bytes / sizeof(wchar_t));
  value->assign(buffer.begin(), buffer.end());
  return S_OK;
}

HRESULT QueryHeadersInt(HINTERNET request_handle,
                        uint32_t info_level,
                        const wchar_t* name,
                        int* value) {
  info_level |= WINHTTP_QUERY_FLAG_NUMBER;
  DWORD num_bytes = sizeof(*value);
  if (!::WinHttpQueryHeaders(request_handle, info_level, name, value,
                             &num_bytes, WINHTTP_NO_HEADER_INDEX)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

}  // namespace updater
