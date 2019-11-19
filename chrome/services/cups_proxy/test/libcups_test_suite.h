// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_
#define CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/test_suite.h"

namespace cups_proxy {

class LibCupsTestSuite : public base::TestSuite {
 public:
  LibCupsTestSuite(int argc, char** argv);
  ~LibCupsTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LibCupsTestSuite);
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_TEST_LIBCUPS_TEST_SUITE_H_
