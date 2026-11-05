// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"

#include <memory>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace omnibox_everywhere {

class OmniboxEverywhereWidgetDelegateTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(OmniboxEverywhereWidgetDelegateTest,
       WindowIconFollowsDefaultSearchEngine) {
  constexpr int kExpectedDipSize =
#if BUILDFLAG(IS_WIN)
      32;
#else
      16;
#endif
  const gfx::Image expected_loupe(gfx::CreateVectorIcon(
      vector_icons::kSearchIcon, kExpectedDipSize, SK_ColorBLACK));

  TemplateURLServiceFactoryTestUtil template_url_service_util(&profile_);
  template_url_service_util.VerifyLoad();
  TemplateURLService* template_url_service = template_url_service_util.model();
  auto set_default_search_provider = [&](bool is_google) {
    TemplateURLData data;
    data.SetShortName(is_google ? u"Google" : u"Other");
    data.SetKeyword(is_google ? u"google.com" : u"other.com");
    data.SetURL(is_google ? "https://www.google.com/search?q={searchTerms}"
                          : "https://www.other.com/search?q={searchTerms}");
    template_url_service->SetUserSelectedDefaultSearchProvider(
        template_url_service->Add(std::make_unique<TemplateURL>(data)));
  };

  // Branded builds show the Google "G" when Google is the default search
  // engine, so the window icon must come from the service, not a null one.
  set_default_search_provider(/*is_google=*/true);
  OmniboxEverywhereWidgetDelegate delegate(template_url_service);
  EXPECT_EQ(gfx::test::AreImagesEqual(delegate.GetWindowIcon().GetImage(),
                                      expected_loupe),
            !BUILDFLAG(GOOGLE_CHROME_BRANDING));

  // The widget is reused across shows, so refreshing the icon picks up a
  // default search engine change made after the delegate was created.
  set_default_search_provider(/*is_google=*/false);
  delegate.UpdateWindowIconForSearchEngine(template_url_service);
  EXPECT_TRUE(gfx::test::AreImagesEqual(delegate.GetWindowIcon().GetImage(),
                                        expected_loupe));

  set_default_search_provider(/*is_google=*/true);
  delegate.UpdateWindowIconForSearchEngine(template_url_service);
  EXPECT_EQ(gfx::test::AreImagesEqual(delegate.GetWindowIcon().GetImage(),
                                      expected_loupe),
            !BUILDFLAG(GOOGLE_CHROME_BRANDING));
}

}  // namespace omnibox_everywhere
