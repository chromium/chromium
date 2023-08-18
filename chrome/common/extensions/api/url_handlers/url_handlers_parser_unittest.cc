// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

namespace extensions {

using UrlHandlersParserUnitTest = ChromeManifestTest;

// Tests an error is properly thrown for a "url_handlers" entry with no
// "matches" specified.
// Regression test for crbug.com/1470739.
TEST_F(UrlHandlersParserUnitTest, EmptyMatches) {
  static constexpr char kMissingMatchesManifest[] =
      R"({
           "name": "UrlHandlers",
           "manifest_version": 2,
           "version": "0.1",
           "app": { "background": { "scripts": ["background.js"] } },
           "url_handlers": {
             "my_url": {
               "title": "My URL",
               "matches": []
             }
           }
         })";

  LoadAndExpectError(ManifestData::FromJSON(kMissingMatchesManifest),
                     "Invalid value for 'url_handlers[my_url].matches'.");
}

}  // namespace extensions
