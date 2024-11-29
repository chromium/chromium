// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/test/fuzztest_support.h"
#include "base/values.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace web_app {
namespace {
void UpdateManifestCanSuccessfullyParseAnyString(const base::Value& json) {
  auto result = UpdateManifest::CreateFromJson(
      json, GURL("https://example.com/manifest.json"));
}

class Environment {
 public:
  Environment() {
    // Sets min log level to FATAL for performance.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    // ICU is as of 2024-11-28 reachable from here so we need to initialize ICU
    // to avoid hitting NOTREACHED()s. See https://crbug.com/381129857.
    CHECK(base::i18n::InitializeICU());
  }
  // Required by ICU integration according to several other fuzzer environments.
  // TODO(pbos): Consider breaking out this fuzzer environment to something
  // shared among most fuzzers. See net/base/fuzzer_test_support.cc and consider
  // moving something like that into the commons.
  base::AtExitManager at_exit_manager;
};

Environment* const environment = new Environment();

FUZZ_TEST(UpdateManifestFuzzTest, UpdateManifestCanSuccessfullyParseAnyString)
    .WithSeeds({*base::JSONReader::Read("{}"), *base::JSONReader::Read(R"({
                  "versions": []
                })"),
                *base::JSONReader::Read(R"({
                  "channels": {},
                  "versions": []
                })"),
                *base::JSONReader::Read(R"({
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn"
                    },
                    {
                      "version": "1.0.3",
                      "url": "bundle.swbn"
                    },
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle2.swbn"
                    }
                  ]
                })"),
                *base::JSONReader::Read(R"({
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn",
                      "blah": 123
                    }
                  ]
                })"),
                *base::JSONReader::Read(R"({
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn",
                      "channels": ["test", "stable", "test"]
                    }
                  ]
                })"),
                *base::JSONReader::Read(R"({
                  "channels": {
                    "test": {
                      "name": "Test Title"
                    },
                    "stable": {
                      "another property": 123
                    },
                    "another channel": {}
                  },
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn",
                      "channels": ["test", "stable", "test"]
                    }
                  ]
                })")});
}  // namespace
}  // namespace web_app
