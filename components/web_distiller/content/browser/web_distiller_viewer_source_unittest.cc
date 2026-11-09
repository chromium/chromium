// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/content/browser/web_distiller_viewer_source.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/distiller.h"
#include "components/web_distiller/core/url_constants.h"
#include "components/web_distiller/core/web_distiller_service.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace web_distiller {

namespace {

// Returns `GURL("chrome-distiller://uuid/path")`.
GURL GetURL(std::string_view path) {
  return GURL(base::StrCat(
      {kWebDistillerScheme, url::kStandardSchemeSeparator, "uuid/", path}));
}

}  // namespace

class WebDistillerViewerSourceTest : public content::RenderViewHostTestHarness {
};

TEST_F(WebDistillerViewerSourceTest, TestMimeType) {
  WebDistillerViewerSource source(nullptr);
  EXPECT_EQ("text/css", source.GetMimeType(GetURL(kViewerCssPath)));
  EXPECT_EQ("text/html", source.GetMimeType(GetURL("anythingelse")));
}

// Tests that the viewer CSP defaults to 'none' and only opens the directives
// distilled articles need.
TEST_F(WebDistillerViewerSourceTest, ContentSecurityPolicy) {
  using network::mojom::CSPDirectiveName;
  WebDistillerViewerSource source(nullptr);
  EXPECT_EQ("default-src 'none';",
            source.GetContentSecurityPolicy(CSPDirectiveName::DefaultSrc));
  EXPECT_EQ("img-src 'self' data: http: https:;",
            source.GetContentSecurityPolicy(CSPDirectiveName::ImgSrc));
  EXPECT_EQ("media-src http: https:;",
            source.GetContentSecurityPolicy(CSPDirectiveName::MediaSrc));
  EXPECT_EQ("font-src https://fonts.gstatic.com;",
            source.GetContentSecurityPolicy(CSPDirectiveName::FontSrc));
  EXPECT_EQ("form-action 'none';",
            source.GetContentSecurityPolicy(CSPDirectiveName::FormAction));
  EXPECT_EQ("base-uri 'none';",
            source.GetContentSecurityPolicy(CSPDirectiveName::BaseURI));
  EXPECT_EQ("style-src 'self' https://fonts.googleapis.com;",
            source.GetContentSecurityPolicy(CSPDirectiveName::StyleSrc));
  EXPECT_EQ("child-src *;",
            source.GetContentSecurityPolicy(CSPDirectiveName::ChildSrc));
  // connect-src is left empty so that it inherits default-src 'none'.
  EXPECT_EQ("", source.GetContentSecurityPolicy(CSPDirectiveName::ConnectSrc));
  EXPECT_EQ("", source.GetContentSecurityPolicy(
                    CSPDirectiveName::RequireTrustedTypesFor));
  EXPECT_EQ("",
            source.GetContentSecurityPolicy(CSPDirectiveName::TrustedTypes));
  // object-src and script-src are inherited from content::URLDataSource.
  // script-src is what blocks script in untrusted distilled markup, so both
  // are pinned exactly.
  EXPECT_EQ("object-src 'none';",
            source.GetContentSecurityPolicy(CSPDirectiveName::ObjectSrc));
  EXPECT_EQ("script-src chrome://resources 'self';",
            source.GetContentSecurityPolicy(CSPDirectiveName::ScriptSrc));
}

// Tests that `kViewerSaveFontScalingPath` requests persist the font scaling
// preference and complete with an empty response rather than falling through
// into the main document request path.
TEST_F(WebDistillerViewerSourceTest,
       TestSaveFontScalingCompletesWithoutViewerCreation) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  DistilledPagePrefs::RegisterProfilePrefs(pref_service.registry());
  WebDistillerService service(
      /*distiller_factory=*/nullptr,
      /*distiller_page_factory=*/nullptr,
      std::make_unique<DistilledPagePrefs>(&pref_service),
      /*distiller_ui_handle=*/nullptr);
  WebDistillerViewerSource source(&service);

  base::test::TestFuture<scoped_refptr<base::RefCountedMemory>> data_future;
  source.StartDataRequest(
      GetURL(base::StrCat({kViewerSaveFontScalingPath, "1.5"})),
      base::BindRepeating(&WebDistillerViewerSourceTest::web_contents,
                          base::Unretained(this)),
      data_future.GetCallback());

  EXPECT_EQ(nullptr, data_future.Get());
  EXPECT_FLOAT_EQ(1.5f, service.GetDistilledPagePrefs()->GetFontScaling());
}

}  // namespace web_distiller
