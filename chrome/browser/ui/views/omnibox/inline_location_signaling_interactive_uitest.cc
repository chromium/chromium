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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/browser/geolocation_header_service_test_api.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
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
  ContentSetting site_permission = CONTENT_SETTING_BLOCK;
  bool use_https = true;
  bool is_precise = false;
  bool has_cached_location = true;
  bool navigate_to_ineligible = false;
  std::string user_input = "a";
  std::string mock_suggest_response;
  std::vector<std::pair<std::u16string, std::u16string>> expected_results;
  std::optional<OmniboxInlineLocationSuggestionShown> expected_shown_state;
  std::optional<size_t> expected_position_metric;
  bool test_parent_click = false;
  bool send_x_geo_header = true;
  GeolocationHeaderPrimeLocationOutcome expected_prime_outcome =
      GeolocationHeaderPrimeLocationOutcome::kNotTriedCachedLocationFresh;
  GeolocationHeaderGetLocationOutcome expected_get_location_outcome =
      GeolocationHeaderGetLocationOutcome::kPermissionStateMismatch;
};

}  // namespace

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
    // If has_cached_location is false, we set a stale timestamp (25 hours old).
    // This ensures that even if a focus flow triggers a geolocation query and
    // updates the cached location via the overrider, the location is treated
    // as stale (age > 24h) and HasCachedLocation() returns false during
    // navigation.
    position->timestamp = GetParam().has_cached_location
                              ? base::Time::Now()
                              : base::Time::Now() - base::Hours(25);
    position->is_precise = GetParam().is_precise;
    return position;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
    // Disable the 200ms debounce timer in SearchProvider for tests to prevent
    // hitting the 1.5s AutocompleteController timeout on slow bots.
    command_line->AppendSwitchASCII("omnibox-suggest-polling-strategy", "0");
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

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InteractiveBrowserTest::CreatedBrowserMainParts(browser_main_parts);

    // Intercept the Device OS Mojo layer early to prevent early startup
    // geolocation queries from binding to the real system service.
    device::mojom::GeopositionResultPtr result_ptr =
        device::mojom::GeopositionResult::NewPosition(CreateMockGeoposition());
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            std::move(result_ptr));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Mock system-level location permission (Mac, Win, ChromeOS).
    system_permission_settings_ =
        std::make_unique<system_permission_settings::ScopedSettingsForTesting>(
            ContentSettingsType::GEOLOCATION, /*blocked=*/false);

    // Safeguard: Guarantee active window widget focus prior to executing
    // subview focus checks
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kSearchSuggestEnabled, true);
    // Privacy Documentation: Anonymized data collection must be granted for
    // `SearchProvider` payload transmissions
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // Surgical Fix: Increase the global provider timeout specifically for this
    // test to avoid flakes on slow bots without affecting production users.
    BrowserWindow::FromBrowser(browser())
        ->GetLocationBar()
        ->GetOmniboxController()
        ->autocomplete_controller()
        ->config_.stop_timer_duration = base::Seconds(10);
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
  std::unique_ptr<system_permission_settings::ScopedSettingsForTesting>
      system_permission_settings_;
};

IN_PROC_BROWSER_TEST_P(InlineLocationSignalingE2EInteractiveUiTest,
                       VerifyE2EOrdering) {
  Profile* profile = browser()->GetProfile();
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

  data.send_x_geo_header = GetParam().send_x_geo_header;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);
  if (GetParam().has_cached_location) {
    GeolocationHeaderServiceTestApi(geo_service)
        .SetLocation(CreateMockGeoposition());
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(
      test_server_->GetURL(kExternalEngineHost, "/"));
  ContentSetting site_perm = GetParam().site_permission;
  settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, site_perm);

  OmniboxController* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                              ->GetLocationBar()
                                              ->GetOmniboxController();
  AutocompleteController* controller =
      omnibox_controller->autocomplete_controller();

  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();

  base::HistogramTester prime_histogram_tester;
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return static_cast<OmniboxViewViews*>(omnibox_view)->HasFocus();
  }));

  ASSERT_TRUE(base::test::RunUntil([&]() { return controller->done(); }));

  // Wait for any asynchronous Mojo geolocation query triggered by the focus
  // flow or DSE change to complete before modifying omnibox state.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !GeolocationHeaderServiceTestApi(geo_service).is_geolocation_bound();
  }));

  prime_histogram_tester.ExpectUniqueSample(
      "Omnibox.GeolocationHeaderService.PrimeLocationOutcome",
      GetParam().expected_prime_outcome, 1);

  base::HistogramTester histogram_tester;
  omnibox_controller->StopAutocomplete(true);
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::UTF8ToUTF16(GetParam().user_input));
  omnibox_view->OnAfterPossibleChange(true);
  if (GetParam().navigate_to_ineligible) {
    EXPECT_TRUE(base::test::RunUntil([&]() { return controller->done(); }));
  } else {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return controller->done() &&
             std::ranges::any_of(controller->result(), [](const auto& match) {
               return match.type == AutocompleteMatchType::SEARCH_SUGGEST;
             });
    }));
  }

  const AutocompleteResult& result = controller->result();

  if (!GetParam().navigate_to_ineligible) {
    // Default background providers append extra elements, loop ensures top
    // slots match layout exactly
    ASSERT_GE(result.size(), GetParam().expected_results.size());

    for (size_t i = 0; i < GetParam().expected_results.size(); ++i) {
      EXPECT_EQ(result.match_at(i).contents,
                GetParam().expected_results[i].first);
      EXPECT_EQ(result.match_at(i).description,
                GetParam().expected_results[i].second);
    }
  }

  std::string permission_str;
  if (site_perm == CONTENT_SETTING_BLOCK) {
    permission_str = "Deny";
  } else if (site_perm == CONTENT_SETTING_ASK) {
    permission_str = "Ask";
  }

  if (GetParam().expected_shown_state.has_value() && !permission_str.empty()) {
    EXPECT_GE(
        histogram_tester.GetBucketCount("Omnibox.InlineLocationSuggestion." +
                                            permission_str + ".ShownState",
                                        *GetParam().expected_shown_state),
        1);
  } else {
    if (!permission_str.empty()) {
      histogram_tester.ExpectTotalCount(
          "Omnibox.InlineLocationSuggestion." + permission_str + ".ShownState",
          0);
    }
  }

  if (GetParam().expected_position_metric.has_value()) {
    EXPECT_GE(histogram_tester.GetBucketCount(
                  "Omnibox.InlineLocationSuggestion.Index",
                  *GetParam().expected_position_metric),
              1);
  } else {
    histogram_tester.ExpectTotalCount("Omnibox.InlineLocationSuggestion.Index",
                                      0);
  }

  // Perform navigation to DSE results page to trigger and verify navigation
  // telemetry.
  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  // Trigger omnibox click navigation to verify click telemetry.
  // 1. Find the parent or the ills suggestion (based on test_parent_click).
  size_t target_line = 0;
  bool suggestion_found = false;

  if (GetParam().test_parent_click) {
    auto parent_it = std::ranges::find_if(result, [](const auto& match) {
      return match.subtypes.contains(
                 omnibox::SUBTYPE_LOCATION_SUGGEST_TRIGGER) &&
             !match.extra_headers.contains(kXGeoHeader);
    });
    if (parent_it != result.end()) {
      target_line = std::distance(result.begin(), parent_it);
      suggestion_found = true;
    }
  } else {
    auto duplicate_it = std::ranges::find_if(result, [](const auto& match) {
      return match.subtypes.contains(
                 omnibox::SUBTYPE_LOCATION_SUGGEST_TRIGGER) &&
             match.extra_headers.contains(kXGeoHeader);
    });
    if (duplicate_it != result.end()) {
      target_line = std::distance(result.begin(), duplicate_it);
      suggestion_found = true;
    }
  }

  // Select and simulate clicking (pressing Return) on the selected match.
  omnibox_controller->edit_model()->SetPopupSelection(
      OmniboxPopupSelection(target_line));
  ASSERT_EQ(omnibox_controller->edit_model()->GetPopupSelection().line,
            target_line);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  navigation_observer.Wait();

  // Verify site permission reset: if the initial permission was DENY and the
  // inline location signaling row was clicked, permission should be reset to
  // ASK.
  if (site_perm == CONTENT_SETTING_BLOCK && suggestion_found &&
      !GetParam().test_parent_click) {
    auto descriptor = blink::mojom::PermissionDescriptor::New(
        blink::mojom::PermissionName::GEOLOCATION, nullptr);
    blink::mojom::PermissionStatus current_status =
        browser()
            ->GetProfile()
            ->GetPermissionControllerDelegate()
            ->GetPermissionStatus(
                descriptor, test_server_->GetURL(kExternalEngineHost, "/"),
                test_server_->GetURL(kExternalEngineHost, "/"));
    EXPECT_EQ(blink::mojom::PermissionStatus::ASK, current_status);
  }

  // Verify navigation telemetry.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.GeolocationHeaderService.GetLocationOutcome.Automatic",
      GetParam().expected_get_location_outcome, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.GeolocationHeaderService.GetLocationOutcome.Automatic", 1);

  if (permission_str.empty()) {
    return;
  }

  // Verify click telemetry if suggestion was found/clicked.
  if (suggestion_found) {
    // 2. If the suggestion is found click it and check metrics
    std::string expected_logged_metric =
        "Omnibox.InlineLocationSuggestion." + permission_str +
        (GetParam().test_parent_click ? ".ParentClicked" : ".Clicked");
    std::string expected_empty_metric =
        "Omnibox.InlineLocationSuggestion." + permission_str +
        (GetParam().test_parent_click ? ".Clicked" : ".ParentClicked");

    // Verify the expected click metric records exactly 1.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return histogram_tester.GetBucketCount(expected_logged_metric, true) == 1;
    }));

    // Verify the other click metric records 0.
    histogram_tester.ExpectTotalCount(expected_empty_metric, 0);
  } else {
    // 3. If the suggestion is not found click 0 and check no metrics.
    histogram_tester.ExpectTotalCount(
        "Omnibox.InlineLocationSuggestion." + permission_str + ".Clicked", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.InlineLocationSuggestion." + permission_str + ".ParentClicked",
        0);
  }
}

const InlineLocationSignalingTestCase kTestCases[] = {
    // 1. Tests Below placement + Approximate wording
    {.test_name = "DisplayBelow_UseApproximateLocation",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2},

    // 2. Tests Below placement + Location wording
    {.test_name = "DisplayBelow_UseLocation",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion", u"Use location"}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2,
     .test_parent_click = true},

    // 3. Tests Above placement + Approximate wording
    {.test_name = "DisplayAbove_UseApproximateLocation",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1,
     .test_parent_click = true},

    // 4. Tests Above placement + Location wording
    {.test_name = "DisplayAbove_UseLocation",
     .display_order = "DisplayAbove",
     .wording = "UseLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u"Use location"},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1},

    // 5. Tests mismatching suggest subtypes do not invoke copying duplication
    {.test_name = "SubtypeMismatchDoesNotDuplicate",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[100]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kNoEligibleSuggestionFound},

    // 6. Tests duplication is skipped if site already has content settings
    // access granted
    {.test_name = "SkippedWhenSitePermissionAllowed",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission = CONTENT_SETTING_ALLOW,
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kTriedQueryNextPosition,
     .expected_get_location_outcome =
         GeolocationHeaderGetLocationOutcome::kSuccess},

    // 7. Tests duplication suppression over insecure connections
    {.test_name = "SkippedOverHttpSchemaConnection",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = false,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kOnlyParentSuggestionShown,
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kNotTriedInvalidUrlOrInsecure,
     .expected_get_location_outcome =
         GeolocationHeaderGetLocationOutcome::kInsecureConnection},

    // 8. Tests that copy duplication triggers strictly on the first eligible
    // match returned
    {.test_name = "OnlyFirstEligibleSubtypeIsDuplicated",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"other-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[457]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"other-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2},

    // 9. Verifies intermediate slot positioning rules (Below layout)
    {.test_name = "IntermediatePlacementBelow",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
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
                          {u"third-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 3},

    // 10. Verifies intermediate slot positioning rules (Above layout)
    {.test_name = "IntermediatePlacementAbove",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
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
                          {u"a query third-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2},

    // 11. Verifies placement logic when first suggestion is the primary
    // candidate target
    {.test_name = "FirstPlacementAbove",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"other-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""},
                          {u"other-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1},

    // 12. Verifies copied suggestion never overrides default index 0 slot
    // verbatim match
    {.test_name = "SignalingMatchNeverTakesIndexZero",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "query",
     .mock_suggest_response = "[\"query\",[\"a-location-relevant-suggestion\"],"
                              "[],[],{\"google:suggestsubtypes\":[[457]],"
                              "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"query", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1},

    // 13. Verifies behavior when verbatim search string itself carries
    // signaling subtypes
    {.test_name = "VerbatimSignalingMatchDoesNotDisplaceDefault",
     .display_order = "DisplayAbove",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a query suggestion",
     .mock_suggest_response =
         "[\"a query suggestion\",[\"a query suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1600]}]",
     .expected_results = {{u"a query suggestion", kExpectedDseSearchText},
                          {u"a query suggestion", u"Use approximate location"}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1},

    // 14. Verifies only the first candidate triggers copy duplication when
    // multiple valid items are loaded
    {.test_name = "MultipleLocationSuggestionsOnlyFirstDuplicated",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response = "[\"a\",[\"a-location-relevant-suggestion\", "
                              "\"another-location-relevant-suggestion\"],[],[],"
                              "{\"google:suggestsubtypes\":[[457],[457]],"
                              "\"google:suggestrelevance\":[1400,1399]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"},
                          {u"another-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2},

    // 15. E2E test for precise location caching + Dynamic accuracy wording.
    // The UI wording must show "Use precise location" regardless of the wording
    // parameter value.
    {.test_name = "PreciseLocation_UseApproximateLocationWordingParam",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .use_https = true,
     .is_precise = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use precise location"}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2,
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kTriedQueryCachedPosition},

    // 16. E2E test for precise location caching + Dynamic accuracy wording.
    // Again, the UI wording must show "Use precise location" even if the
    // wording param is "UseLocation".
    {.test_name = "PreciseLocation_UseLocationWordingParam",
     .display_order = "DisplayAbove",
     .wording = "UseLocation",
     .use_https = true,
     .is_precise = true,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion",
                           u"Use precise location"},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 1,
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kTriedQueryCachedPosition},

    // 17. E2E test verifying that setting permission to ASK correctly logs
    // to the Ask shown state UMA histogram.
    {.test_name = "DisplayBelow_UseApproximateLocation_AskPermission",
     .display_order = "DisplayBelow",
     .wording = "UseApproximateLocation",
     .site_permission = CONTENT_SETTING_ASK,
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""},
                          {u"a-location-relevant-suggestion",
                           u"Use approximate location"}},
     .expected_shown_state =
         OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown,
     .expected_position_metric = 2,
     .test_parent_click = true},

    // 18. Telemetry verification: No cached location.
    // Site has permission allowed, DSE uses HTTPS, but device has no location
    // cached/primed. Expected outcome: kNoCachedLocation (1).
    {.test_name = "Telemetry_NoCachedLocation",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission = CONTENT_SETTING_ALLOW,
     .use_https = true,
     .is_precise = false,
     .has_cached_location = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}},
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kTriedQueryNextPosition,
     .expected_get_location_outcome =
         GeolocationHeaderGetLocationOutcome::kNoCachedLocation},

    // 19. Telemetry verification: Ineligible URL.
    // Site has permission allowed, device has cached location, but the user
    // types a non-DSE URL to navigate. Expected outcome: kIneligibleUrl (4).
    {.test_name = "Telemetry_IneligibleUrl",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission = CONTENT_SETTING_ALLOW,
     .use_https = true,
     .is_precise = false,
     .navigate_to_ineligible = true,
     .user_input = "https://www.yahoo.com",
     .mock_suggest_response = "",
     .expected_results = {},
     .expected_prime_outcome =
         GeolocationHeaderPrimeLocationOutcome::kTriedQueryNextPosition,
     .expected_get_location_outcome =
         GeolocationHeaderGetLocationOutcome::kIneligibleUrl},

    // 20. Telemetry verification: Default Search Provider does not accept
    // header.
    // DSE has send_x_geo_header disabled. Expected outcome:
    // kNotTriedProviderDoesNotAcceptHeader (2) for priming and kIneligibleUrl
    // (4) for get location.
    {.test_name = "Telemetry_ProviderDoesNotAcceptHeader",
     .display_order = "DisplayBelow",
     .wording = "UseLocation",
     .site_permission = CONTENT_SETTING_ALLOW,
     .use_https = true,
     .is_precise = false,
     .user_input = "a",
     .mock_suggest_response =
         "[\"a\",[\"a-location-relevant-suggestion\"],[],[],"
         "{\"google:suggestsubtypes\":[[457]],"
         "\"google:suggestrelevance\":[1400]}]",
     .expected_results = {{u"a", kExpectedDseSearchText},
                          {u"a-location-relevant-suggestion", u""}},
     .send_x_geo_header = false,
     .expected_prime_outcome = GeolocationHeaderPrimeLocationOutcome::
         kNotTriedProviderDoesNotAcceptHeader,
     .expected_get_location_outcome =
         GeolocationHeaderGetLocationOutcome::kIneligibleUrl},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InlineLocationSignalingE2EInteractiveUiTest,
    testing::ValuesIn(kTestCases),
    [](const testing::TestParamInfo<
        InlineLocationSignalingE2EInteractiveUiTest::ParamType>& info) {
      return info.param.test_name;
    });
