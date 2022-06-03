// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LIB_UTIL_H_
#define CHROME_UPDATER_LIB_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"

namespace updater {

// This is a lightweight implementation of net::UnescapeURLComponent to avoid a
// dependency on //net using the following rules: NORMAL | SPACES |
// PATH_SEPARATORS | URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS.
//
// |escaped_text| should only contain ASCII text.
//
// Returns back |escaped_text| if unescaping failed.
std::string UnescapeURLComponent(base::StringPiece escaped_text);

}  // namespace updater

#endif  // CHROME_UPDATER_LIB_UTIL_H_
