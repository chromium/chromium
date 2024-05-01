// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"

#include <memory>
#include <string_view>

#include "base/strings/strcat.h"
#include "components/dom_distiller/core/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace dom_distiller {

namespace {

// Returns `GURL("chrome-distiller://uuid/path")`.
GURL GetURL(std::string_view path) {
  return GURL(base::StrCat(
      {kDomDistillerScheme, url::kStandardSchemeSeparator, "uuid/", path}));
}

}  // namespace

class DomDistillerViewerSourceTest : public testing::Test {};

TEST_F(DomDistillerViewerSourceTest, TestMimeType) {
  DomDistillerViewerSource source(nullptr);
  EXPECT_EQ("text/css", source.GetMimeType(GetURL(kViewerCssPath)));
  EXPECT_EQ("text/html", source.GetMimeType(GetURL("anythingelse")));
}

}  // namespace dom_distiller
