// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"

#if defined(OS_WIN)
#include <memory>

#include "base/win/scoped_com_initializer.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
#endif
  base::TestSuite test_suite(argc, argv);
  chrome::RegisterPathProvider();
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
