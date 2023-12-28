// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_SUITE_AURA_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_SUITE_AURA_H_

#include "base/memory/raw_ptr.h"
#include "base/test/test_suite.h"
#include "ui/gl/gl_display.h"

namespace exo {
namespace test {

// Simple implementation of a TestSuite with minimal external dependencies. This
// is built directly on top of Aura. Useful for generic features that are built
// on top of Aura and do not need a full platform Shell implementation (such as
// ash::Shell).
class ExoTestSuiteAura : public base::TestSuite {
 public:
  ExoTestSuiteAura(int argc, char** argv);

  ExoTestSuiteAura(const ExoTestSuiteAura&) = delete;
  ExoTestSuiteAura& operator=(const ExoTestSuiteAura&) = delete;

  ~ExoTestSuiteAura() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_SUITE_AURA_H_
