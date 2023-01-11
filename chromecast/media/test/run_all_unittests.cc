// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "mojo/core/embedder/embedder.h"

class CastMediaTestSuite : public base::TestSuite {
 public:
  // Note: the base class constructor creates an AtExitManager.
  CastMediaTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}
  ~CastMediaTestSuite() override {}

 protected:
  void Initialize() override;
};

void CastMediaTestSuite::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

  // Initialize the FFMpeg library.
  // Note: at this time, AtExitManager is already present.
  media::InitializeMediaLibrary();
}

int main(int argc, char** argv) {
  CastMediaTestSuite test_suite(argc, argv);
  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&CastMediaTestSuite::Run, base::Unretained(&test_suite)));
}
