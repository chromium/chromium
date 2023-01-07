// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/test/libcups_test_suite.h"

#include "chrome/services/cups_proxy/test/paths.h"

namespace cups_proxy {

LibCupsTestSuite::LibCupsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

LibCupsTestSuite::~LibCupsTestSuite() = default;

void LibCupsTestSuite::Initialize() {
  base::TestSuite::Initialize();
  Paths::RegisterPathProvider();
}

}  // namespace cups_proxy
