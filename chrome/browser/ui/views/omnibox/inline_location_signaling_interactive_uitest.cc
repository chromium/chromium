// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "url/gurl.h"

namespace {

constexpr char kExternalEngineHost[] = "127.0.0.1";
const std::u16string kExpectedDseSearchText = u"Test DSE Search";
constexpr double kMockLatitude = 37.3861;
constexpr double kMockLongitude = -122.0839;

struct InlineLocationSignalingTestCase {
  std::string test_name;
  std::string display_order;
  std::string wording;
  bool site_permission_allowed = false;
  bool use_https = true;
  std::string user_input = "a";
  std::string mock_suggest_response;
  std::vector<std::pair<std::u16string, std::u16string>> expected_results;
};

class InlineLocationSignalingE2EInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<InlineLocationSignalingTestCase> {
 public:
  InlineLocationSignalingE2EInteractiveUiTest() = default;

  device::mojom::GeopositionPtr CreateMockGeoposition() {
    device::mojom::GeopositionPtr position = device::mojom::Geoposition::New();
    position->latitude = kMockLatitude;
    position->longitude = kMockLongitude;
    position->accuracy = 1.0;
    position->timestamp = base::Time::Now();
    position->is_precise = false;
    return position;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
  }

  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kInlineLocationSignaling,
          {{"display_order", GetParam().display_order},
           {"wording", GetParam().wording}}},
         {omnibox::kPlatformAgnosticXGeo, {}}},
        {::features::kInitialWebUI,
         ::features::kWebUIToolbarProcessOverheadExperiment});

    // Architectural Fix: Initialize the mock environment early so startup WebUI
    // toolbar autocomplete requests are intercepted immediately instead of
    // blocking on uninitialized paths.
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        GetParam().use_https ? net::EmbeddedTestServer::TYPE_HTTPS
                             : net::EmbeddedTestServer::TYPE_HTTP);
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &InlineLocationSignalingE2EInteractiveUiTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_->Start());

    // Overrider moved to test body to ensure mojo environment is fully ready.
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Safeguard: Guarantee active window widget focus prior to executing
    // subview focus checks
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled,
                                                 true);
    // Privacy Documentation: Anonymized data collection must be granted for
    // `SearchProvider` payload transmissions
    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.starts_with("/suggest?q=")) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(GetParam().mock_suggest_response);
      return response;
    }
    return nullptr;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_P(InlineLocationSignalingE2EInteractiveUiTest,
                       VerifyE2EOrdering) {
  Profile* profile = browser()->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

  TemplateURLData data;
  data.SetShortName(u"Test DSE");
  data.SetKeyword(u"testdse");

  std::string base_url = test_server_->GetURL(kExternalEngineHost, "/").spec();
  if (base::EndsWith(base_url, "/")) {
    base_url.pop_back();
  }

  if (GetParam().use_https) {
    data.SetURL(base_url + "/search?q={searchTerms}");
    data.suggestions_url = base_url + "/suggest?q={searchTerms}";
  } else {
    GURL url(base_url);
    GURL::Replacements replacements;
    replacements.SetSchemeStr("http");
    GURL http_url = url.ReplaceComponents(replacements);
    std::string http_base_url = http_url.spec();
    if (base::EndsWith(http_base_url, "/")) {
      http_base_url.pop_back();
    }
    data.SetURL(http_base_url + "/search?q={searchTerms}");
    data.suggestions_url = base_url + "/suggest?q={searchTerms}";
  }

  data.send_x_geo_header = true;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);
  geo_service->SetLocationForTesting(CreateMockGeoposition());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(
      test_server_->GetURL(kExternalEngineHost, "/"));
  ContentSetting site_perm = GetParam().site_permission_allowed
                                 ? CONTENT_SETTING_ALLOW
                                 : CONTENT_SETTING_BLOCK;
  settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, site_perm);

  OmniboxController* omnibox_controller =
      browser()->window()->GetLocationBar()->GetOmniboxController();
  AutocompleteController* controller =
      omnibox_controller->autocomplete_controller();

  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return static_cast<OmniboxViewViews*>(omnibox_view)->HasFocus();
  }));

  ASSERT_TRUE(base::test::RunUntil([&]() { return controller->done(); }));

  // Late Setup: Intercept the Device OS Mojo layer now that mojo is booted,
  // matching the profile cache coordinates.
  device::mojom::GeopositionResultPtr result_ptr =
      device::mojom::GeopositionResult::NewPosition(CreateMockGeoposition());
  geolocation_overrider_ = std::make_unique<device::ScopedGeolocationOverrider>(
      std::move(result_ptr));

  omnibox_controller->StopAutocomplete(true);
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::UTF8ToUTF16(GetParam().user_input));
  omnibox_view->OnAfterPossibleChange(true);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return controller->done() &&
           std::ranges::any_of(controller->result(), [](const auto& match) {
             return match.type == AutocompleteMatchType::SEARCH_SUGGEST;
           });
  }));

  const AutocompleteResult& result = controller->result();

  // Default background providers append extra elements, loop ensures top slots
  // match layout exactly
  ASSERT_GE(result.size(), GetParam().expected_results.size());

  for (size_t i = 0; i < GetParam().expected_results.size(); ++i) {
    EXPECT_EQ(result.match_at(i).contents,
              GetParam().expected_results[i].first);
    EXPECT_EQ(result.match_at(i).description,
              GetParam().expected_results[i].second);
  }
}

const InlineLocationSignalingTestCase kTestCases[] = {
    // 1. Tests Below placement + Approximate wording
    {.test_name = "DisplayBelow_UseApproximateLocation",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"}}},

    // 2. Tests Below placement + Location wording
    {.test_name = "DisplayBelow_UseLocation",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use location"}}},

    // 3. Tests Above placement + Approximate wording
    {.test_name = "DisplayAbove_UseApproximateLocation",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""}}},

    // 4. Tests Above placement + Location wording
    {.test_name = "DisplayAbove_UseLocation",
     .display_order = "DisplayAbove",
     .wording = "UseLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u"Use location"},
                          {u"a-location-relevant-suggestion", u""}}},

    // 5. Tests mismatching suggest subtypes do not invoke copying duplication
    {.test_name = "SubtypeMismatchDoesNotDuplicate",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[100]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}}},

    // 6. Tests duplication is skipped if site already has content settings
    // access granted
    {.test_name = "SkippedWhenSitePermissionAllowed",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission_allowed = true,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}}},

    // 7. Tests duplication suppression over insecure connections
    {.test_name = "SkippedOverHttpSchemaConnection",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}}},

    // 8. Tests that copy duplication triggers strictly on the first eligible
    // match returned
    {.test_name = "OnlyFirstEligibleSubtypeIsDuplicated",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"other-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[457]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"other-suggestion", u""}}},

    // 9. Verifies intermediate slot positioning rules (Below layout)
    {.test_name = "IntermediatePlacementBelow",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"other-suggestion\", "
                              "\"a-location-relevant-suggestion\", "
                              "\"third-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[],[457],[]],"
                              "\"google:suggestrelevance\":[1600,1599,1598]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"other-suggestion", u""},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"third-suggestion", u""}}},

    // 10. Verifies intermediate slot positioning rules (Above layout)
    {.test_name = "IntermediatePlacementAbove",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a query",
     .mock_suggest_response = "[\"a query\",[\"a query other-suggestion\", "
                              "\"a query location-relevant-suggestion\", "
                              "\"a query third-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[],[457],[]],"
                              "\"google:suggestrelevance\":[1600,1599,1598]}]",
     .expected_results = {{u"a query", kExpectedDseSearchText},
                          {u"a query other-suggestion", u""},
                          {u"a query location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a query location-relevant-suggestion", u""},
                          {u"a query third-suggestion", u""}}},

    // 11. Verifies placement logic when first suggestion is the primary
    // candidate target
    {.test_name = "FirstPlacementAbove",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"other-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""},
                          {u"other-suggestion", u""}}},

    // 12. Verifies copied suggestion never overrides default index 0 slot
    // verbatim match
    {.test_name = "SignalingMatchNeverTakesIndexZero",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "query",
     .mock_suggest_response = "[\"query\",[\"a-location-relevant-suggestion\"],"
                              "[],[],{\"google:suggestsubtypes\":[[457]],"
                              "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"query", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""}}},

    // 13. Verifies behavior when verbatim search string itself carries
    // signaling subtypes
    {.test_name = "VerbatimSignalingMatchDoesNotDisplaceDefault",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a query suggestion",
     .mock_suggest_response =
         "[\"a query suggestion\",[\"a query suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1600]}]",
     .expected_results = {{u"a query suggestion", kExpectedDseSearchText},
                          {u"a query suggestion",
                           u"Use approximate location"}}},

    // 14. Verifies only the first candidate triggers copy duplication when
    // multiple valid items are loaded
    {.test_name = "MultipleLocationSuggestionsOnlyFirstDuplicated",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission_allowed = false,
     .use_https = true,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"another-location-relevant-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[457]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"another-location-relevant-suggestion", u""}}},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InlineLocationSignalingE2EInteractiveUiTest,
    testing::ValuesIn(kTestCases),
    [](const testing::TestParamInfo<
        InlineLocationSignalingE2EInteractiveUiTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
