// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_

#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_test.h"

// Base class for unit tests that load manifest data from Chrome TEST_DATA_DIR.
// TODO(jamescook): Move this class and all subclasses into the extensions
// namespace.
class ChromeManifestTest : public extensions::ManifestTest {
 public:
  ChromeManifestTest();

  ChromeManifestTest(const ChromeManifestTest&) = delete;
  ChromeManifestTest& operator=(const ChromeManifestTest&) = delete;

  ~ChromeManifestTest() override;

  // ManifestTest overrides:
  base::FilePath GetTestDataDir() override;

 private:
  // Force the manifest tests to run as though they are on trunk, since several
  // tests rely on manifest features being available that aren't on
  // stable/beta.
  //
  // These objects nest, so if a test wants to explicitly test the behaviour
  // on stable or beta, declare it inside that test.
  extensions::ScopedCurrentChannel current_channel_;
};

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_
