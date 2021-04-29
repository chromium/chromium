// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lib_util.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_full.h"

#include <wininet.h>  // For INTERNET_MAX_URL_LENGTH.

namespace updater {

std::string UnescapeURLComponent(base::StringPiece escaped_text_piece) {
  if (escaped_text_piece.empty())
    return {};

  std::string escaped_text(escaped_text_piece);

  // UrlUnescapeA doesn't modify the buffer unless passed URL_UNESCAPE_INPLACE.
  char* escaped_text_ptr = const_cast<char*>(escaped_text.data());

  DWORD buf_len = INTERNET_MAX_URL_LENGTH;
  std::vector<char> unescaped_text_buf(buf_len);
  HRESULT res =
      ::UrlUnescapeA(escaped_text_ptr, unescaped_text_buf.data(), &buf_len, 0);
  CHECK(res != E_POINTER);
  CHECK_LE(buf_len, INTERNET_MAX_URL_LENGTH);

  if (FAILED(res))
    return escaped_text;

  return {unescaped_text_buf.data(), buf_len};
}

}  // namespace updater
