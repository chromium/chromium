// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_
#define CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace cups_proxy {

class LibCupsTestSuite : public base::TestSuite {
 public:
  LibCupsTestSuite(int argc, char** argv);

  LibCupsTestSuite(const LibCupsTestSuite&) = delete;
  LibCupsTestSuite& operator=(const LibCupsTestSuite&) = delete;

  ~LibCupsTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_
