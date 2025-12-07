// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DefaultEnvironment final : public ::testing::Environment {
 public:
  DefaultEnvironment() = default;

  void SetUp() override { CreateWrlModule(); }
};

}  // namespace

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  install_static::ScopedInstallDetails scoped_install_details;

  testing::AddGlobalTestEnvironment(new DefaultEnvironment);

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
