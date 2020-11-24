// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/secure_dns_config.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

using testing::_;
using testing::IsEmpty;
using testing::Return;

namespace settings {

namespace {

constexpr char kGetSecureDnsResolverList[] = "getSecureDnsResolverList";
constexpr char kParseCustomDnsEntry[] = "parseCustomDnsEntry";
constexpr char kProbeCustomDnsTemplate[] = "probeCustomDnsTemplate";
constexpr char kRecordUserDropdownInteraction[] =
    "recordUserDropdownInteraction";
constexpr char kWebUiFunctionName[] = "webUiCallbackName";

net::DohProviderEntry::List GetDohProviderListForTesting() {
  static const auto global1 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global1", net::DohProviderIdForHistogram(-1), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://global1.provider/dns-query{?dns}",
      "Global Provider 1" /* ui_name */,
      "https://global1.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  static const auto no_display = net::DohProviderEntry::ConstructForTesting(
      "Provider_NoDisplay", net::DohProviderIdForHistogram(-2), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://nodisplay.provider/dns-query{?dns}",
      "No Display Provider" /* ui_name */,
      "https://nodisplay.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {} /* display_countries */);
  static const auto ee_fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_EE_FR", net::DohProviderIdForHistogram(-3), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://ee.fr.provider/dns-query{?dns}",
      "EE/FR Provider" /* ui_name */,
      "https://ee.fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"EE", "FR"} /* display_countries */);
  static const auto fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_FR", net::DohProviderIdForHistogram(-4), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://fr.provider/dns-query{?dns}",
      "FR Provider" /* ui_name */,
      "https://fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"FR"} /* display_countries */);
  static const auto global2 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global2", net::DohProviderIdForHistogram(-5), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://global2.provider/dns-query{?dns}",
      "Global Provider 2" /* ui_name */,
      "https://global2.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  return {&global1, &no_display, &ee_fr, &fr, &global2};
}

bool FindDropdownItem(const base::Value& resolvers,
                      const std::string& name,
                      const std::string& value,
                      const std::string& policy) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("name", base::Value(name));
  dict.SetKey("value", base::Value(value));
  dict.SetKey("policy", base::Value(policy));

  return std::find(resolvers.GetList().begin(), resolvers.GetList().end(),
                   dict) != resolvers.GetList().end();
}

}  // namespace

class TestSecureDnsHandler : public SecureDnsHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using SecureDnsHandler::set_web_ui;
};

class SecureDnsHandlerTest : public InProcessBrowserTest {
 protected:
#if defined(OS_WIN)
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
    ON_CALL(provider_, IsInitializationComplete(_)).WillByDefault(Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(_))
        .WillByDefault(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestSecureDnsHandler>();
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  // Updates out-params from the last message sent to WebUI about a secure DNS
  // change. Returns false if the message was invalid or not found.
  bool GetLastSettingsChangedMessage(
      std::string* secure_dns_mode,
      std::vector<std::string>* secure_dns_templates,
      int* management_mode) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->is_string() ||
          data->arg1()->GetString() != "secure-dns-setting-changed") {
        continue;
      }

      const base::DictionaryValue* dict = nullptr;
      if (!data->arg2()->GetAsDictionary(&dict))
        return false;

      // Get the secure DNS mode.
      if (!dict->FindStringPath("mode"))
        return false;
      *secure_dns_mode = *dict->FindStringPath("mode");

      // Get the secure DNS templates.
      if (!dict->FindListPath("templates"))
        return false;
      secure_dns_templates->clear();
      for (const auto& template_str :
           dict->FindListPath("templates")->GetList()) {
        if (!template_str.is_string())
          return false;
        secure_dns_templates->push_back(template_str.GetString());
      }

      // Get the forced management description.
      if (!dict->FindIntPath("managementMode"))
        return false;
      *management_mode = *dict->FindIntPath("managementMode");

      return true;
    }
    return false;
  }

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
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SecureDnsHandlerTest);
};

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsModes) {
  PrefService* local_state = g_browser_process->local_state();
  std::string secure_dns_mode;
  std::vector<std::string> secure_dns_templates;
  int management_mode;

  local_state->SetString(prefs::kDnsOverHttpsMode, SecureDnsConfig::kModeOff);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeSecure, secure_dns_mode);

  local_state->SetString(prefs::kDnsOverHttpsMode, "unknown");
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicy) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeAutomatic));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);

  std::string secure_dns_mode;
  std::vector<std::string> secure_dns_templates;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsPolicyChange) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeAutomatic));

  std::string secure_dns_mode;
  std::vector<std::string> secure_dns_templates;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeAutomatic, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);

  SetPolicyForPolicyKey(&policy_map, policy::key::kDnsOverHttpsMode,
                        base::Value(SecureDnsConfig::kModeOff));
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kNoOverride),
            management_mode);
}

// On platforms where enterprise policies do not have default values, test
// that DoH is disabled when non-DoH policies are set.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, OtherPoliciesSet) {
  policy::PolicyMap policy_map;
  SetPolicyForPolicyKey(&policy_map, policy::key::kIncognitoModeAvailability,
                        base::Value(1));

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);

  std::string secure_dns_mode;
  std::vector<std::string> secure_dns_templates;
  int management_mode;
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(SecureDnsConfig::kModeOff, secure_dns_mode);
  EXPECT_EQ(static_cast<int>(SecureDnsConfig::ManagementMode::kDisabledManaged),
            management_mode);
}
#endif

// This test makes no assumptions about the country or underlying resolver list.
IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, DropdownList) {
  base::ListValue args;
  args.AppendString(kWebUiFunctionName);

  web_ui_.HandleReceivedMessage(kGetSecureDnsResolverList, &args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Check results.
  base::Value::ConstListView resolver_list = call_data.arg3()->GetList();
  ASSERT_GE(resolver_list.size(), 1U);
  EXPECT_TRUE(resolver_list[0].FindKey("value")->GetString().empty());
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, DropdownListContents) {
  const auto entries = GetDohProviderListForTesting();
  handler_->SetProvidersForTesting(entries);
  const base::Value resolver_list = handler_->GetSecureDnsResolverList();

  EXPECT_EQ(entries.size() + 1, resolver_list.GetList().size());
  EXPECT_TRUE(resolver_list.GetList()[0].FindKey("value")->GetString().empty());
  for (const auto* entry : entries) {
    EXPECT_TRUE(FindDropdownItem(resolver_list, entry->ui_name,
                                 entry->dns_over_https_template,
                                 entry->privacy_policy));
  }
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, DropdownListChange) {
  handler_->SetProvidersForTesting(GetDohProviderListForTesting());

  base::HistogramTester histograms;
  base::ListValue args;
  args.AppendString(std::string() /* old_provider */);
  args.AppendString(
      "https://global1.provider/dns-query{?dns}" /* new_provider */);
  web_ui_.HandleReceivedMessage(kRecordUserDropdownInteraction, &args);

  const std::string kUmaBase = "Net.DNS.UI.DropdownSelectionEvent";
  histograms.ExpectTotalCount(kUmaBase + ".Ignored", 4u);
  histograms.ExpectTotalCount(kUmaBase + ".Selected", 1u);
  histograms.ExpectTotalCount(kUmaBase + ".Unselected", 1u);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, SecureDnsTemplates) {
  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";

  std::string secure_dns_mode;
  std::vector<std::string> secure_dns_templates;
  int management_mode;
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_post_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(1u, secure_dns_templates.size());
  EXPECT_EQ(good_post_template, secure_dns_templates[0]);

  local_state->SetString(prefs::kDnsOverHttpsTemplates,
                         good_post_template + " " + good_get_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(2u, secure_dns_templates.size());
  EXPECT_EQ(good_post_template, secure_dns_templates[0]);
  EXPECT_EQ(good_get_template, secure_dns_templates[1]);

  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(0u, secure_dns_templates.size());

  local_state->SetString(prefs::kDnsOverHttpsTemplates,
                         bad_template + " " + good_post_template);
  EXPECT_TRUE(GetLastSettingsChangedMessage(
      &secure_dns_mode, &secure_dns_templates, &management_mode));
  EXPECT_EQ(1u, secure_dns_templates.size());
  EXPECT_EQ(good_post_template, secure_dns_templates[0]);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateValid) {
  base::ListValue args;
  args.AppendString(kWebUiFunctionName);
  args.AppendString("https://example.template/dns-query");

  base::HistogramTester histograms;
  web_ui_.HandleReceivedMessage(kParseCustomDnsEntry, &args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data.arg2()->GetBool());
  // The template should be valid.
  auto result = call_data.arg3()->GetList();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(result[0].GetString(), "https://example.template/dns-query");
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 0);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 1);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, TemplateInvalid) {
  base::ListValue args;
  args.AppendString(kWebUiFunctionName);
  args.AppendString("invalid_template");

  base::HistogramTester histograms;
  web_ui_.HandleReceivedMessage(kParseCustomDnsEntry, &args);
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data.arg2()->GetBool());
  // The template should be invalid.
  EXPECT_THAT(call_data.arg3()->GetList(), IsEmpty());
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 1);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 0);
}

IN_PROC_BROWSER_TEST_F(SecureDnsHandlerTest, MultipleTemplates) {
  base::HistogramTester histograms;
  base::ListValue args_valid;
  args_valid.AppendString(kWebUiFunctionName);
  args_valid.AppendString(
      "https://example1.template/dns    https://example2.template/dns-query");
  web_ui_.HandleReceivedMessage(kParseCustomDnsEntry, &args_valid);
  const content::TestWebUI::CallData& call_data_valid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_valid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_valid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_valid.arg2()->GetBool());
  // Both templates should be valid.
  auto result = call_data_valid.arg3()->GetList();
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(result[0].GetString(), "https://example1.template/dns");
  EXPECT_EQ(result[1].GetString(), "https://example2.template/dns-query");
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", false, 0);
  histograms.ExpectBucketCount("Net.DNS.UI.ValidationAttemptSuccess", true, 1);

  base::ListValue args_invalid;
  args_invalid.AppendString(kWebUiFunctionName);
  args_invalid.AppendString("invalid_template https://example.template/dns");
  web_ui_.HandleReceivedMessage(kParseCustomDnsEntry, &args_invalid);
  const content::TestWebUI::CallData& call_data_invalid =
      *web_ui_.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data_invalid.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data_invalid.arg1()->GetString());
  // The request should be successful.
  ASSERT_TRUE(call_data_invalid.arg2()->GetBool());
  // The entry should be invalid.
  EXPECT_THAT(call_data_invalid.arg3()->GetList(), IsEmpty());
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
  base::ListValue args_valid;
  args_valid.AppendString(kWebUiFunctionName);
  args_valid.AppendString("https://example.template/dns-query");
  web_ui_.HandleReceivedMessage(kProbeCustomDnsTemplate, &args_valid);
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
  base::ListValue args_valid;
  args_valid.AppendString(kWebUiFunctionName);
  args_valid.AppendString("https://example.template/dns-query");
  web_ui_.HandleReceivedMessage(kProbeCustomDnsTemplate, &args_valid);
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
  base::ListValue args_valid;
  args_valid.AppendString(kWebUiFunctionName);
  args_valid.AppendString("https://example.template/dns-query");
  // Request a probe that will hang.
  handler_->SetNetworkContextForTesting(network_context_hang.get());
  web_ui_.HandleReceivedMessage(kProbeCustomDnsTemplate, &args_valid);
  size_t responses = web_ui_.call_data().size();
  base::RunLoop().RunUntilIdle();
  // No response yet from the hanging probe.
  EXPECT_EQ(responses, web_ui_.call_data().size());

  // Request a probe that will fail.
  handler_->SetNetworkContextForTesting(network_context_fail.get());
  web_ui_.HandleReceivedMessage(kProbeCustomDnsTemplate, &args_valid);
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
