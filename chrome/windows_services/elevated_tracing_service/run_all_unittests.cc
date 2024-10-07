// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "chrome/install_static/test/scoped_install_details.h"

namespace {

class TracingServiceTestSuite : public base::TestSuite {
 public:
  TracingServiceTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv) {}

 private:
  install_static::ScopedInstallDetails install_details_;
};

}  // namespace

int main(int argc, char** argv) {
  base::PlatformThread::SetName("MainThread");

  TracingServiceTestSuite test_suite(argc, argv);

  // Some tests in this binary mutate system-wide state (e.g., to install a
  // Windows service), so run all tests serially to avoid races between tests.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
