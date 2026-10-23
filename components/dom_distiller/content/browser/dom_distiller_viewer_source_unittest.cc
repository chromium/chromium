// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_renderer_host.h"
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

class DomDistillerViewerSourceTest : public content::RenderViewHostTestHarness {
};

TEST_F(DomDistillerViewerSourceTest, TestMimeType) {
  DomDistillerViewerSource source(nullptr);
  EXPECT_EQ("text/css", source.GetMimeType(GetURL(kViewerCssPath)));
  EXPECT_EQ("text/html", source.GetMimeType(GetURL("anythingelse")));
}

// Tests that `kViewerSaveFontScalingPath` requests persist the font scaling
// preference and complete with an empty response rather than falling through
// into the main document request path.
TEST_F(DomDistillerViewerSourceTest,
       TestSaveFontScalingCompletesWithoutViewerCreation) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  DistilledPagePrefs::RegisterProfilePrefs(pref_service.registry());
  DomDistillerService service(
      /*distiller_factory=*/nullptr,
      /*distiller_page_factory=*/nullptr,
      std::make_unique<DistilledPagePrefs>(&pref_service),
      /*distiller_ui_handle=*/nullptr);
  DomDistillerViewerSource source(&service);

  base::test::TestFuture<scoped_refptr<base::RefCountedMemory>> data_future;
  source.StartDataRequest(
      GetURL(base::StrCat({kViewerSaveFontScalingPath, "1.5"})),
      base::BindRepeating(&DomDistillerViewerSourceTest::web_contents,
                          base::Unretained(this)),
      data_future.GetCallback());

  EXPECT_EQ(nullptr, data_future.Get());
  EXPECT_FLOAT_EQ(1.5f, service.GetDistilledPagePrefs()->GetFontScaling());
}

}  // namespace dom_distiller
