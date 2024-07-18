// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include "base/json/json_reader.h"
#include "base/test/fuzztest_support.h"
#include "base/values.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace web_app {
namespace {
void UpdateManifestCanSuccessfullyParseAnyString(const base::Value& json) {
  auto result = UpdateManifest::CreateFromJson(
      json, GURL("https://example.com/manifest.json"));
}

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
