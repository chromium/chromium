// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_URI_UNITTEST_H_
#define CHROMEOS_PRINTING_URI_UNITTEST_H_

#include <string>
#include <utility>
#include <vector>

#include "chromeos/printing/uri.h"

// This file contains a declaration of struct and constant used only in the
// implementation of unit tests for class Uri declared in uri.h. This file is
// not supposed to be included anywhere outside the files uri_unittest*.cc.

namespace chromeos {

// All printable ASCII characters ('"' and '\' are escaped with \).
constexpr char kPrintableASCII[] =
    " !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~";

namespace uri_unittest {

// A simple structure with all URI components.
struct UriComponents {
  std::string scheme;
  std::string userinfo;
  std::string host;
  int port = -1;  // -1 means "unspecified"
  std::vector<std::string> path;
  std::vector<std::pair<std::string, std::string>> query;
  std::string fragment;
  UriComponents();
  UriComponents(const UriComponents&);
  UriComponents(
      const std::string& scheme,
      const std::string& userinfo,
      const std::string& host,
      int port = -1,
      const std::vector<std::string>& path = {},
      const std::vector<std::pair<std::string, std::string>>& query = {},
      const std::string& fragment = "");
  ~UriComponents();
};

}  // namespace uri_unittest

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_URI_UNITTEST_H_
