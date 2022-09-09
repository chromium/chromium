// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_POLICY_PATH_PARSER_H_
#define CHROME_INSTALL_STATIC_POLICY_PATH_PARSER_H_

#include <string>

namespace install_static {

// See documentation in chrome/browser/policy/policy_path_parser.h for details
// on the variables expanded for all platforms, and for Windows specifically.
std::wstring ExpandPathVariables(const std::wstring& untranslated_string);

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_POLICY_PATH_PARSER_H_
