// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NET_UTIL_H_
#define CHROME_UPDATER_WIN_NET_NET_UTIL_H_

#include <windows.h>
#include <winhttp.h>

#include <stdint.h>

#include <string>

#include "base/check_op.h"
#include "base/strings/string16.h"
#include "chrome/updater/win/util.h"

namespace updater {

HRESULT QueryHeadersString(HINTERNET request_handle,
                           uint32_t info_level,
                           const base::char16* name,
                           base::string16* value);

HRESULT QueryHeadersInt(HINTERNET request_handle,
                        uint32_t info_level,
                        const base::char16* name,
                        int* value);

// Queries WinHTTP options for the given |handle|. Returns S_OK if the call
// is successful.
template <typename T>
HRESULT QueryOption(HINTERNET handle, uint32_t option, T* value) {
  auto num_bytes = sizeof(*value);
  if (!::WinHttpQueryOption(handle, option, value, &num_bytes)) {
    DCHECK_EQ(sizeof(*value), num_bytes);
    return HRESULTFromLastError();
  }
  return S_OK;
}

// Sets WinHTTP options for the given |handle|. Returns S_OK if the call
// is successful.
template <typename T>
HRESULT SetOption(HINTERNET handle, uint32_t option, T value) {
  if (!::WinHttpSetOption(handle, option, &value, sizeof(value)))
    return HRESULTFromLastError();
  return S_OK;
}

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NET_UTIL_H_
