// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_MIME_UTILS_H_
#define COMPONENTS_EXO_MIME_UTILS_H_

#include <string>

namespace exo {

constexpr char kEncodingUTF8Legacy[] = "UTF8_STRING";

// Takes a text/* mime type and returns the name of the character set specified
// in the type. If no character set is specified, defaults to US-ASCII.
std::string GetCharset(const std::string& mime_type);

}  // namespace exo

#endif  // COMPONENTS_EXO_MIME_UTILS_H_
