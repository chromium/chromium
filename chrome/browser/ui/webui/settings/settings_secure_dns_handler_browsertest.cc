// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#endif

using testing::_;
using testing::IsEmpty;
using testing::Return;

namespace settings {

namespace {

constexpr char kGetSecureDnsResolverList[] = "getSecureDnsResolverList";
constexpr char kIsValidConfig[] = "isValidConfig";
constexpr char kProbeConfig[] = "probeConfig";
constexpr char kWebUiFunctionName[] = "webUiCallbackName";

net::DohProviderEntry::List GetDohProviderListForTesting() {
  static BASE_FEATURE(kDohProviderFeatureForProvider_Global1,
                      "DohProviderFeatureForProvider_Global1",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static const auto global1 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global1", &kDohProviderFeatureForProvider_Global1,
      {} /*ip_strs */, {} /* dot_hostnames */,
      "https://global1.provider/dns-query{?dns}",
      "Global Provider 1" /* ui_name */,
      "https://global1.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  static BASE_FEATURE(kDohProviderFeatureForProvider_NoDisplay,
                      "DohProviderFeatureForProvider_NoDisplay",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static const auto no_display = net::DohProviderEntry::ConstructForTesting(
      "Provider_NoDisplay", &kDohProviderFeatureForProvider_NoDisplay,
      {} /*ip_strs */, {} /* dot_hostnames */,
      "https://nodisplay.provider/dns-query{?dns}",
      "No Display Provider" /* ui_name */,
      "https://nodisplay.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {} /* display_countries */);
  static BASE_FEATURE(kDohProviderFeatureForProvider_EE_FR,
                      "DohProviderFeatureForProvider_EE_FR",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static const auto ee_fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_EE_FR", &kDohProviderFeatureForProvider_EE_FR, {} /*ip_strs */,
      {} /* dot_hostnames */, "https://ee.fr.provider/dns-query{?dns}",
      "EE/FR Provider" /* ui_name */,
      "https://ee.fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"EE", "FR"} /* display_countries */);
  static BASE_FEATURE(kDohProviderFeatureForProvider_FR,
                      "DohProviderFeatureForProvider_FR",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static const auto fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_FR", &kDohProviderFeatureForProvider_FR, {} /*ip_strs */,
      {} /* dot_hostnames */, "https://fr.provider/dns-query{?dns}",
      "FR Provider" /* ui_name */,
      "https://fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"FR"} /* display_countries */);
  static BASE_FEATURE(kDohProviderFeatureForProvider_Global2,
                      "DohProviderFeatureForProvider_Global2",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static const auto global2 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global2", &kDohProviderFeatureForProvider_Global2,
      {} /*ip_strs */, {} /* dot_hostnames */,
      "https://global2.provider/dns-query{?dns}",
      "Global Provider 2" /* ui_name */,
      "https://global2.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  return {&global1, &no_display, &ee_fr, &fr, &global2};
}

bool FindDropdownItem(const base::Value::List& resolvers,
                      const std::string& name,
                      const std::string& value,
                      const std::string& policy) {
  base::Value::Dict dict;
  dict.Set("name", name);
  dict.Set("value", value);
  dict.Set("policy", policy);

  return base::Contains(resolvers, dict);
}

}  // namespace

class TestSecureDnsHandler : public SecureDnsHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using SecureDnsHandler::set_web_ui;
};

class SecureDnsHandlerTest : public InProcessBrowserTest {
 public:
  SecureDnsHandlerTest(const SecureDnsHandlerTest&) = delete;
  SecureDnsHandlerTest& operator=(const SecureDnsHandlerTest&) = delete;

 protected:
#if BUILDFLAG(IS_WIN)
  SecureDnsHandlerTest()
      // Mark as not enterprise managed to prevent the secure DNS mode from
      // being downgraded to off.
      : scoped_domain_(false) {}
#else
  SecureDnsHandlerTest() = default;
#endif
  ~SecureDnsHandlerTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    // Initialize user policy.
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestSecureDnsHandler>();
    web_ui_.set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  // Updates out-params from the last message sent to WebUI about a secure DNS
  // change. Returns false if the message was invalid or not found.
  bool GetLastSettingsChangedMessage(std::string* out_secure_dns_mode,
                                     std::string* out_doh_config,
                                     int* out_management_mode) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->is_string() ||
          data->arg1()->GetString() != "secure-dns-setting-changed") {
        continue;
      }

      const base::Value::Dict* dict = data->arg2()->GetIfDict();
      if (!dict)
        return false;

      // Get the secure DNS mode.
      const std::string* secure_dns_mode = dict->FindString("mode");
      if (!secure_dns_mode)
        return false;
      *out_secure_dns_mode = *secure_dns_mode;

      // Get the DoH config string.
      const std::string* doh_config = dict->FindString("config");
      if (!doh_config)
        return false;
      *out_doh_config = *doh_config;

      // Get the forced management description.
      std::optional<int> management_mode = dict->FindInt("managementMode");
      if (!management_mode.has_value())
        return false;
      *out_management_mode = *management_mode;

      return true;
    }
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Similar to `GetLastSettingsChangedMessage`, but only reads data related to
  // template URIs with identifiers.  Returns false if the message was invalid
  // or not found; in this case the out params may be not set.
  bool GetIdentifierConfigsFromLastSettingsChangedMessage(
      bool* out_doh_with_identifiers_active,
      std::string* out_doh_config_for_display) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->is_string() ||
          data->arg1()->GetString() != "secure-dns-setting-changed") {
        continue;
      }
      const base::Value::Dict* dict = data->arg2()->GetIfDict();
      if (!dict)
        return false;
      std::optional<bool> doh_with_identifiers_active =
          dict->FindBool("dohWithIdentifiersActive");
      if (!doh_with_identifiers_active)
        return false;
      *out_doh_with_identifiers_active = *doh_with_identifiers_active;

      const std::string* doh_config_for_display =
          dict->FindString("configForDisplay");
      if (!doh_config_for_display)
        return false;
      *out_doh_config_for_display = *doh_config_for_display;

      return true;
    }
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Sets a policy update which will cause power pref managed change.
  void SetPolicyForPolicyKey(policy::PolicyMap* policy_map,
                             const std::string& policy_key,
                             base::Value value) {
    policy_map->Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::move(value), nullptr);
    provider_.UpdateChromePolicy(*policy_map);
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<TestSecureDnsHandler> handler_;
  content::TestWebUI web_ui_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

 private:
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain_;
#endif
};

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsModes) {
  PrefService* local_state = g_browser_process->local_state();
  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;

  PrefService* pref_service_for_user_settings = local_state;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the local_state is shared between all users so the user-set
  // pref is stored in the profile's pref service.
  pref_service_for_user_settings = browser()->profile()->GetPrefs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeOff);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeAutomatic);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeSecure);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeSecure, secure_dns_mode);

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            "unknown");
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicy) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the local_state is only used on managed profiles.
  g_browser_process->platform_part()
      ->secure_dns_manager()
      ->SetPrimaryProfilePropertiesForTesting(browser()->profile()->GetPrefs(),
                                              /*is_profile_managed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeAutomatic));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);

  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicyChange) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the local_state is only used on managed profiles.
  g_browser_process->platform_part()
      ->secure_dns_manager()
      ->SetPrimaryProfilePropertiesForTesting(browser()->profile()->GetPrefs(),
                                              /*is_profile_managed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeAutomatic));

  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);

  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeOff));
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);
}

// On platforms where enterprise policies do not have default values, test
// that DoH is disabled when non-DoH policies are set.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, OtherPoliciesSet) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kIncognitoModeAvailability,
                        base::Value(1));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);

  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kDisabledManaged),
            management_mode);
}
#endif

// This test makes no assumptions about the country or underlying resolver list.
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, DropdownList) {
  base::Value::List args;
  args.Append(kWebUiFunctionName);

  web_ui_.HandleReceivedMessage(kGetSecureDnsResolverList, args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Check results (no providers set for testing).
  const base::Value::List& resolver_list = call_data.arg3()->GetList();
  ASSERT_GE(resolver_list.size(), 0U);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, DropdownListContents) {
  const auto entries = GetDohProviderListForTesting();
  handler_->SetProvidersForTesting(entries);
  const base::Value::List resolver_list = handler_->GetSecureDnsResolverList();

  EXPECT_EQ(entries.size(), resolver_list.size());
  for (const net::DohProviderEntry* entry : entries) {
    EXPECT_TRUE(FindDropdownItem(resolver_list, entry->ui_name,
                                 entry->doh_server_config.server_template(),
                                 entry->privacy_policy));
  }
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsTemplates) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const std::string kDnsOverHttpsTemplatesPrefName =
      prefs::kDnsOverHttpsEffectiveTemplatesChromeOS;
#else
  const std::string kDnsOverHttpsTemplatesPrefName =
      prefs::kDnsOverHttpsTemplates;
#endif

  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";

  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;
  PrefService* pref_service_for_user_settings =
      g_browser_process->local_state();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the local_state is shared between all users so the user-set
  // pref is stored in the profile's pref service.
  pref_service_for_user_settings = browser()->profile()->GetPrefs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeAutomatic);
  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            good_post_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(good_post_template, doh_config);
  std::string two_templates = good_post_template + "\n" + good_get_template;
  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            two_templates);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(two_templates, doh_config);

  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            bad_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_THAT(doh_config, IsEmpty());

  pref_service_for_user_settings->SetString(
      kDnsOverHttpsTemplatesPrefName, bad_template + " " + good_post_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(good_post_template, doh_config);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest,
                       SecureDnsTemplatesWithIdentifiers) {
  std::string templatesWithIdentifier =
      "https://foo.test-${USER_EMAIL}/dns-query{?dns}";
  std::string templatesWithIdentifierDisplay =
      "https://foo.test-${stub-user@example.com}/dns-query{?dns}";
  std::string templatesWithIdentifierEffective =
      "https://"
      "foo.test-"
      "A3AB66F42D4B8C81160D04124BFFF7B197C9B10EB04BB4E75DBE0E3FFCF39FA4/"
      "dns-query{?dns}";
  std::string templates = "https://bar.test/dns-query{?dns}";

  g_browser_process->platform_part()
      ->secure_dns_manager()
      ->SetPrimaryProfilePropertiesForTesting(browser()->profile()->GetPrefs(),
                                              /*is_profile_managed=*/true);

  std::string secure_dns_mode;
  std::string doh_config, doh_config_for_display;
  bool doh_with_identifiers_active;
  int management_mode;

  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsTemplates,
                        base::Value(templates));
  SetPolicyForPolicyKey(&policy_map,
                        policy::key::kDnsOverHttpsTemplatesWithIdentifiers,
                        base::Value(templatesWithIdentifier));
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsSalt,
                        base::Value("salt-for-test"));
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_TRUE(GetIdentifierConfigsFromLastSettingsChangedMessage(
      &doh_with_identifiers_active, &doh_config_for_display));
  EXPECT_EQ(templatesWithIdentifierEffective, doh_config);
  EXPECT_EQ(templatesWithIdentifierDisplay, doh_config_for_display);
  EXPECT_TRUE(doh_with_identifiers_active);

  SetPolicyForPolicyKey(&policy_map,
                        policy::key::kDnsOverHttpsTemplatesWithIdentifiers,
                        base::Value());
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_TRUE(GetIdentifierConfigsFromLastSettingsChangedMessage(
      &doh_with_identifiers_active, &doh_config_for_display));
  EXPECT_EQ(templates, doh_config);
  EXPECT_FALSE(doh_with_identifiers_active);
}

// Unmanaged users store the secure DoH config as profile prefs.
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest,
                       SecureDnsTemplatesForUnmanagedUsers) {
  const char kTemplates[] = "https://test1/dns-query{?dns}";
  const char kTemplatesAlt[] = "https://test2/dns-query{?dns}";

  PrefService* local_state = g_browser_process->local_state();
  PrefService* profile_prefs = browser()->profile()->GetPrefs();

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, kTemplates);

  std::string secure_dns_mode;
  std::string doh_config;
  int management_mode;

  EXPECT_FALSE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                             &management_mode));

  profile_prefs->SetString(prefs::kDnsOverHttpsMode,
                           SecureDnsConfig::kModeSecure);
  profile_prefs->SetString(prefs::kDnsOverHttpsTemplates, kTemplates);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(secure_dns_mode, SecureDnsConfig::kModeSecure);
  EXPECT_EQ(doh_config, kTemplates);

  profile_prefs->SetString(prefs::kDnsOverHttpsMode,
                           SecureDnsConfig::kModeAutomatic);
  profile_prefs->SetString(prefs::kDnsOverHttpsTemplates, kTemplatesAlt);
  EXPECT_TRUE(GetLastSettingsChangedMessage(&secure_dns_mode, &doh_config,
                                            &management_mode));
  EXPECT_EQ(secure_dns_mode, SecureDnsConfig::kModeAutomatic);
  EXPECT_EQ(doh_config, kTemplatesAlt);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateValid) {
  base::Value::List args;
  args.Append(kWebUiFunctionName);
  args.Append("https://example.template/dns-query");

  base::HistogramTester histograms;
  web_ui_.HandleReceivedMessage(kIsValidConfig, args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data.arg2()->GetBool());
  // The template should be valid.
  EXPECT_TRUE(call_data.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 0);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 1);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateInvalid) {
  base::Value::List args;
  args.Append(kWebUiFunctionName);
  args.Append("invalid_template");

  base::HistogramTester histograms;
  web_ui_.HandleReceivedMessage(kIsValidConfig, args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data.arg2()->GetBool());
  // The template should be invalid.
  EXPECT_FALSE(call_data.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 1);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 0);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, MultipleTemplates) {
  base::HistogramTester histograms;
  base::Value::List args_valid;
  args_valid.Append(kWebUiFunctionName);
  args_valid.Append(
      "https://example1.template/dns    https://example2.template/dns-query");
  web_ui_.HandleReceivedMessage(kIsValidConfig, args_valid);
  const content::TestWebUI::CallData& call_data_valid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_valid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_valid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_valid.arg2()->GetBool());
  // Both templates should be valid.
  EXPECT_TRUE(call_data_valid.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 0);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 1);

  base::Value::List args_invalid;
  args_invalid.Append(kWebUiFunctionName);
  args_invalid.Append("invalid_template https://example.template/dns");
  web_ui_.HandleReceivedMessage(kIsValidConfig, args_invalid);
  const content::TestWebUI::CallData& call_data_invalid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_invalid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_invalid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_invalid.arg2()->GetBool());
  // The entry should be invalid.
  EXPECT_FALSE(call_data_invalid.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 1);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 1);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateProbeSuccess) {
  auto network_context_ =
      std::make_unique<chrome_browser_net::FakeHostResolverNetworkContext>(
          std::vector<chrome_browser_net::FakeHostResolver::SingleResult>(
              {chrome_browser_net::FakeHostResolver::SingleResult(
                  net::OK, net::ResolveErrorInfo(net::OK),
                  chrome_browser_net::FakeHostResolver::
                      kOneAddressResponse)}) /* current_config_result_list */,
          std::vector<chrome_browser_net::FakeHostResolver::
                          SingleResult>() /* google_config_result_list */);
  handler_->SetNetworkContextForTesting(network_context_.get());
  base::HistogramTester histograms;
  base::Value::List args_valid;
  args_valid.Append(kWebUiFunctionName);
  args_valid.Append("https://example.template/dns-query https://example2/");
  web_ui_.HandleReceivedMessage(kProbeConfig, args_valid);
  base::RunLoop().RunUntilIdle();

  const content::TestWebUI::CallData& call_data_valid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_valid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_valid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_valid.arg2()->GetBool());
  // The probe query should have succeeded.
  ASSERT_TRUE(call_data_valid.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", false, 0);
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", true, 1);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateProbeFailure) {
  auto network_context_ =
      std::make_unique<chrome_browser_net::FakeHostResolverNetworkContext>(
          std::vector<chrome_browser_net::FakeHostResolver::SingleResult>(
              {chrome_browser_net::FakeHostResolver::SingleResult(
                  net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_MALFORMED_RESPONSE),
                  chrome_browser_net::FakeHostResolver::
                      kNoResponse)}) /* current_config_result_list */,
          std::vector<chrome_browser_net::FakeHostResolver::
                          SingleResult>() /* google_config_result_list */);
  handler_->SetNetworkContextForTesting(network_context_.get());
  base::HistogramTester histograms;
  base::Value::List args_valid;
  args_valid.Append(kWebUiFunctionName);
  args_valid.Append("https://example.template/dns-query");
  web_ui_.HandleReceivedMessage(kProbeConfig, args_valid);
  base::RunLoop().RunUntilIdle();

  const content::TestWebUI::CallData& call_data_valid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_valid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_valid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_valid.arg2()->GetBool());
  // The probe query should have failed.
  ASSERT_FALSE(call_data_valid.arg3()->GetBool());
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", false, 1);
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", true, 0);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateProbeDebounce) {
  auto network_context_hang =
      std::make_unique<chrome_browser_net::HangingHostResolverNetworkContext>();
  auto network_context_fail =
      std::make_unique<chrome_browser_net::FakeHostResolverNetworkContext>(
          std::vector<chrome_browser_net::FakeHostResolver::SingleResult>(
              {chrome_browser_net::FakeHostResolver::SingleResult(
                  net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_MALFORMED_RESPONSE),
                  chrome_browser_net::FakeHostResolver::
                      kNoResponse)}) /* current_config_result_list */,
          std::vector<chrome_browser_net::FakeHostResolver::
                          SingleResult>() /* google_config_result_list */);
  base::HistogramTester histograms;
  base::Value::List args_valid;
  args_valid.Append(kWebUiFunctionName);
  args_valid.Append("https://example.template/dns-query");
  // Request a probe that will hang.
  handler_->SetNetworkContextForTesting(network_context_hang.get());
  web_ui_.HandleReceivedMessage(kProbeConfig, args_valid);
  size_t responses = web_ui_.call_data().size();
  base::RunLoop().RunUntilIdle();
  // No response yet from the hanging probe.
  EXPECT_EQ(responses, web_ui_.call_data().size());

  // Request a probe that will fail.
  handler_->SetNetworkContextForTesting(network_context_fail.get());
  web_ui_.HandleReceivedMessage(kProbeConfig, args_valid);
  // The hanging response should now have arrived.
  EXPECT_EQ(responses + 1, web_ui_.call_data().size());
  const content::TestWebUI::CallData& first_response =
      *web_ui_.call_data().back();
  ASSERT_EQ("cr.webUIResponse", first_response.function_name());
  ASSERT_EQ(kWebUiFunctionName, first_response.arg1()->GetString());
  // The cancelled probe should indicate no error.
  ASSERT_TRUE(first_response.arg2()->GetBool());
  EXPECT_TRUE(first_response.arg3()->GetBool());

  // Wait for the second response.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(responses + 2, web_ui_.call_data().size());
  const content::TestWebUI::CallData& second_response =
      *web_ui_.call_data().back();
  ASSERT_EQ("cr.webUIResponse", second_response.function_name());
  ASSERT_EQ(kWebUiFunctionName, second_response.arg1()->GetString());
  // The second request should have resolved with a failure indication.
  ASSERT_TRUE(second_response.arg2()->GetBool());
  EXPECT_FALSE(second_response.arg3()->GetBool());
  // Only the second response should be counted in the histograms.
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", false, 1);
  histograms.ExpectBucketCount("Net.DNS.UI.ProbeAttemptSuccess", true, 0);
}

}  // namespace settings
