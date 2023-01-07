// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/cfi_buildflags.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/simple_feature.h"
#include "ui/shell_dialogs/select_file_dialog.h"          // nogncheck
#include "ui/shell_dialogs/select_file_dialog_factory.h"  // nogncheck
#include "ui/shell_dialogs/select_file_policy.h"          // nogncheck
#else
#include "chrome/browser/toolbar_manager_test_helper_android.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using testing::_;
using testing::Return;

namespace {

// Allows waiting until the policy schema for a |PolicyNamespace| has been made
// available by a |Profile|'s |SchemaRegistry|.
class PolicySchemaAvailableWaiter : public policy::SchemaRegistry::Observer {
 public:
  PolicySchemaAvailableWaiter(Profile* profile,
                              const policy::PolicyNamespace& policy_namespace)
      : registry_(profile->GetPolicySchemaRegistryService()->registry()),
        policy_namespace_(policy_namespace) {}

  PolicySchemaAvailableWaiter(const PolicySchemaAvailableWaiter&) = delete;
  PolicySchemaAvailableWaiter& operator=(const PolicySchemaAvailableWaiter&) =
      delete;

  ~PolicySchemaAvailableWaiter() override { registry_->RemoveObserver(this); }

  // Starts waiting for a policy schema to be available for the
  // |policy_namespace_| that has been passed to the constructor. Returns
  // immediately if the policy schema is already available.
  void Wait() {
    if (RegistryHasSchemaForNamespace())
      return;
    registry_->AddObserver(this);
    run_loop_.Run();
  }

 private:
  bool RegistryHasSchemaForNamespace() {
    const policy::ComponentMap* map =
        registry_->schema_map()->GetComponents(policy_namespace_.domain);
    if (!map)
      return false;
    return map->find(policy_namespace_.component_id) != map->end();
  }

  // policy::SchemaRegistry::Observer:
  void OnSchemaRegistryUpdated(bool has_new_schemas) override {
    if (RegistryHasSchemaForNamespace())
      run_loop_.Quit();
  }

  const raw_ptr<policy::SchemaRegistry> registry_;
  const policy::PolicyNamespace policy_namespace_;
  base::RunLoop run_loop_;
};

std::vector<std::string> PopulateExpectedPolicy(
    const std::string& name,
    const std::string& value,
    const std::string& source,
    const policy::PolicyMap::Entry* policy_map_entry,
    bool unknown) {
  std::vector<std::string> expected_policy;

  // Populate expected policy name.
  expected_policy.push_back(name);

  // Populate expected policy value.
  expected_policy.push_back(value);

  // Populate expected source name.
  expected_policy.push_back(source);

  // Populate expected scope.
  if (policy_map_entry) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        policy_map_entry->scope == policy::POLICY_SCOPE_MACHINE
            ? IDS_POLICY_SCOPE_DEVICE
            : IDS_POLICY_SCOPE_USER));
  } else {
    expected_policy.emplace_back();
  }

  // Populate expected level.
  if (policy_map_entry) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        policy_map_entry->level == policy::POLICY_LEVEL_RECOMMENDED
            ? IDS_POLICY_LEVEL_RECOMMENDED
            : IDS_POLICY_LEVEL_MANDATORY));
  } else {
    expected_policy.emplace_back();
  }

  // Populate expected status.
  if (unknown)
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_LABEL_ERROR));
  else if (!policy_map_entry)
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_UNSET));
  else
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_OK));
  return expected_policy;
}

#if !BUILDFLAG(IS_ANDROID)
void SetChromeMetaData(base::Value::Dict& expected) {
  // Only set the expected keys and types and not the values since
  // these can vary greatly on the platform, OS, architecture
  // that is running.
  expected.SetByDottedPath("chromeMetadata.application", "");
  expected.SetByDottedPath("chromeMetadata.version", "");
  expected.SetByDottedPath("chromeMetadata.revision", "");
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  expected.SetByDottedPath("chromeMetadata.OS", "");
#endif
}

void SetExpectedPolicy(base::Value::Dict& expected,
                       const std::string& name,
                       const std::string& level,
                       const std::string& scope,
                       const std::string& source,
                       const std::string& error,
                       const std::string& warning,
                       bool ignored,
                       const base::Value& value) {
  base::Value::Dict* dict =
      expected.EnsureDict("chromePolicies")->EnsureDict(name.c_str());
  dict->Set("level", level);
  dict->Set("scope", scope);
  dict->Set("source", source);
  if (!error.empty())
    dict->Set("error", error);
  if (!warning.empty())
    dict->Set("warning", warning);
  if (ignored)
    dict->Set("ignored", ignored);
  dict->Set("value", value.Clone());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// The temporary directory and file paths for policy saving.
base::ScopedTempDir export_policies_test_dir;
base::FilePath export_policies_test_file_path;

}  // namespace

class PolicyUITest : public PlatformBrowserTest {
 public:
  PolicyUITest();

  PolicyUITest(const PolicyUITest&) = delete;
  PolicyUITest& operator=(const PolicyUITest&) = delete;

  ~PolicyUITest() override;

 protected:
  // PlatformBrowserTest implementation.
  void SetUpInProcessBrowserTestFixture() override;

  // Uses the |MockConfiguratonPolicyProvider| installed for testing to publish
  // |policy| for |policy_namespace|.
  void UpdateProviderPolicyForNamespace(
      const policy::PolicyNamespace& policy_namespace,
      const policy::PolicyMap& policy);

  void VerifyPolicies(const std::vector<std::vector<std::string>>& expected);

  void VerifyReportButton(bool visible);

  void VerifyExportingPolicies(const base::Value::Dict& expected);

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

#if !BUILDFLAG(IS_ANDROID)
// An artificial SelectFileDialog that immediately returns the location of test
// file instead of showing the UI file picker.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(ui::SelectFileDialog::Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    listener_->FileSelected(export_policies_test_file_path, 0, nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override {}

  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 private:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy));
  }
};
#endif  // !BUILDFLAG(IS_ANDROID)

PolicyUITest::PolicyUITest() {
#if BUILDFLAG(IS_ANDROID)
  // Skips recreating the Android activity when homepage settings are changed.
  // This happens when the feature chrome::android::kStartSurfaceAndroid is
  // enabled.
  toolbar_manager::setSkipRecreateForTesting(true);
#endif  // BUILDFLAG(IS_ANDROID)
}

PolicyUITest::~PolicyUITest() = default;

void PolicyUITest::SetUpInProcessBrowserTestFixture() {
  provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                              /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  policy::PushProfilePolicyConnectorProviderForTesting(&provider_);

  // Create a directory for testing exporting policies.
  ASSERT_TRUE(export_policies_test_dir.CreateUniqueTempDir());
  const std::string filename = "policy.json";
  export_policies_test_file_path =
      export_policies_test_dir.GetPath().AppendASCII(filename);
}

void PolicyUITest::UpdateProviderPolicyForNamespace(
    const policy::PolicyNamespace& policy_namespace,
    const policy::PolicyMap& policy) {
  policy::PolicyBundle bundle;
  bundle.Get(policy_namespace) = policy.Clone();
  provider_.UpdatePolicy(std::move(bundle));
}

void PolicyUITest::VerifyPolicies(
    const std::vector<std::vector<std::string>>& expected_policies) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUIPolicyURL)));

  // Retrieve the text contents of the policy table cells for all policies.
  const std::string javascript =
      "var entries = getAllPolicyTables();"
      "var policies = [];"
      "for (var i = 0; i < entries.length; ++i) {"
      "  var items = getAllPolicyRows(entries[i]);"
      "  for (var j = 0; j < items.length; ++j) {"
      "    var children = getAllPolicyRowDivs(items[j]);"
      "    var values = [];"
      "    for(var k = 0; k < children.length - 1; ++k) {"
      "      values.push(children[k].textContent.trim());"
      "    }"
      "    policies.push(values);"
      "  }"
      "}"
      "domAutomationController.send(JSON.stringify(policies));";
  std::string json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(web_contents(), javascript,
                                                     &json));
  absl::optional<base::Value> value_ptr = base::JSONReader::Read(json);
  ASSERT_TRUE(value_ptr);
  ASSERT_TRUE(value_ptr->is_list());
  const base::Value::List& actual_policies = value_ptr->GetList();

  // Verify that the cells contain the expected strings for all policies.
  ASSERT_EQ(expected_policies.size(), actual_policies.size());
  for (size_t i = 0; i < expected_policies.size(); ++i) {
    const std::vector<std::string> expected_policy = expected_policies[i];
    ASSERT_TRUE(actual_policies[i].is_list());
    const base::Value::List& actual_policy = actual_policies[i].GetList();
    ASSERT_EQ(expected_policy.size(), actual_policy.size());
    for (size_t j = 0; j < expected_policy.size(); ++j) {
      const std::string* value = actual_policy[j].GetIfString();
      ASSERT_TRUE(value);
      if (expected_policy[j] != *value)
        EXPECT_EQ(expected_policy[j], *value);
    }
  }
}

void PolicyUITest::VerifyReportButton(bool visible) {
  const std::string kJavaScript =
      "domAutomationController.send(getReportButtonVisibility());";
  std::string ret;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(web_contents(),
                                                     kJavaScript, &ret));
  EXPECT_EQ(visible, ret != "none");
}

#if !BUILDFLAG(IS_ANDROID)
void PolicyUITest::VerifyExportingPolicies(const base::Value::Dict& expected) {
  // Set SelectFileDialog to use our factory.
  ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory());

  // Navigate to the about:policy page.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUIPolicyURL)));

  // Click on 'save policies' button.
  const std::string javascript =
      "document.getElementById('export-policies').click()";
  EXPECT_TRUE(content::ExecuteScript(web_contents(), javascript));

  base::ThreadPoolInstance::Get()->FlushForTesting();
  // Open the created file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(
      base::ReadFileToString(export_policies_test_file_path, &file_contents));

  absl::optional<base::Value> value = base::JSONReader::Read(file_contents);

  // Check that the file contains a valid dictionary.
  EXPECT_TRUE(value);
  base::Value::Dict* dict = value->GetIfDict();
  EXPECT_TRUE(dict);

  // Since Chrome Metadata has a lot of variations based on platform, OS,
  // architecture and version, it is difficult to test for exact values. Test
  // instead that the same keys exist in the meta data and also that the type of
  // all the keys is a string. The incoming |expected| value should already be
  // filled with the expected keys.
  base::Value::Dict* chrome_metadata = dict->FindDict("chromeMetadata");
  EXPECT_NE(chrome_metadata, nullptr);

  // The |chrome_metadata| we compare against will have the actual values so
  // those will be cleared to empty values so that the equals comparison below
  // will just compare key existence and value types.
  for (auto key_value : *chrome_metadata)
    key_value.second = base::Value(key_value.second.type());

  // Since policy management status can have variable information based on the
  // test bot(e.g., AD joined bot can have updater domain information), it is
  // difficult to test for exact values. Test instead that the same key,
  // "status" exist and also that the type of it is a dictionary. The incoming
  // |expected| value should already have a "status" key with an empty
  // dictionary value.
  base::Value::Dict* status = dict->FindDict("status");
  EXPECT_NE(status, nullptr);
  status->clear();

  // Check that this dictionary is the same as expected.
  EXPECT_EQ(expected, *dict);
}

#if !defined(NDEBUG) ||                                          \
    (BUILDFLAG(IS_LINUX) &&                                      \
     (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
      BUILDFLAG(CFI_ENFORCEMENT_TRAP) ||                         \
      BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC)))
// Slow in debug and CFI builds crbug.com/1338642
#define MAYBE_WritePoliciesToJSONFile DISABLED_WritePoliciesToJSONFile
#else
#define MAYBE_WritePoliciesToJSONFile WritePoliciesToJSONFile
#endif
IN_PROC_BROWSER_TEST_F(PolicyUITest, MAYBE_WritePoliciesToJSONFile) {
  // Set policy values and generate expected dictionary.
  policy::PolicyMap values;
  base::Value::Dict expected_values;

  SetChromeMetaData(expected_values);

  base::Value::List popups_blocked_for_urls;
  popups_blocked_for_urls.Append("aaa");
  popups_blocked_for_urls.Append("bbb");
  popups_blocked_for_urls.Append("ccc");
  values.Set(policy::key::kPopupsBlockedForUrls, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             base::Value(popups_blocked_for_urls.Clone()), nullptr);
  SetExpectedPolicy(expected_values, policy::key::kPopupsBlockedForUrls,
                    "mandatory", "machine", "platform", std::string(),
                    std::string(), false,
                    base::Value(popups_blocked_for_urls.Clone()));

  values.Set(policy::key::kDefaultImagesSetting, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
             base::Value(2), nullptr);
  SetExpectedPolicy(expected_values, policy::key::kDefaultImagesSetting,
                    "mandatory", "machine", "cloud", std::string(),
                    std::string(), false, base::Value(2));

  // This also checks that we save complex policies correctly.
  base::Value::Dict unknown_policy;
  base::Value::Dict* body = unknown_policy.EnsureDict("body");
  body->Set("first", 0);
  body->Set("second", true);
  unknown_policy.Set("head", 12);
  const std::string kUnknownPolicy = "NoSuchThing";
  values.Set(kUnknownPolicy, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(unknown_policy.Clone()), nullptr);
  SetExpectedPolicy(expected_values, kUnknownPolicy, "recommended", "user",
                    "cloud", l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN),
                    std::string(), false,
                    base::Value(std::move(unknown_policy)));

  // Set the extension policies to an empty dictionary as we haven't added any
  // such policies.
  expected_values.Set("extensionPolicies", base::Value::Dict());
  expected_values.Set("status", base::Value::Dict());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  expected_values.Set("loginScreenExtensionPolicies", base::Value::Dict());
  expected_values.Set("deviceLocalAccountPolicies", base::Value::Dict());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  provider_.UpdateChromePolicy(values);

  // Check writing those policies to a newly created file.
  VerifyExportingPolicies(expected_values);

  // Change policy values.
  values.Erase(policy::key::kDefaultImagesSetting);
  expected_values.RemoveByDottedPath(
      std::string("chromePolicies.") +
      std::string(policy::key::kDefaultImagesSetting));

  popups_blocked_for_urls.Append("ddd");
  values.Set(policy::key::kPopupsBlockedForUrls, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             base::Value(popups_blocked_for_urls.Clone()), nullptr);
  SetExpectedPolicy(expected_values, policy::key::kPopupsBlockedForUrls,
                    "mandatory", "machine", "platform", std::string(),
                    std::string(), false,
                    base::Value(popups_blocked_for_urls.Clone()));

  provider_.UpdateChromePolicy(values);

  // Check writing changed policies to the same file (should overwrite the
  // contents).
  VerifyExportingPolicies(expected_values);

#if !BUILDFLAG(IS_CHROMEOS)
  // This also checks that we do not bypass the policy that blocks file
  // selection dialogs. This is a desktop only policy.
  values.Set(policy::key::kAllowFileSelectionDialogs,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_PLATFORM, base::Value(false), nullptr);

  popups_blocked_for_urls.Append("eeeeee");
  values.Set(policy::key::kPopupsBlockedForUrls, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             base::Value(popups_blocked_for_urls.Clone()), nullptr);
  provider_.UpdateChromePolicy(values);

  // Check writing changed policies did not overwrite the exported policies
  // because the file selection dialog is not allowed.
  VerifyExportingPolicies(expected_values);
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PolicyUIStatusTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
    // By default DeviceStateMixin sets public key version to 17 whereas policy
    // test server inside LoggedInUserMixin has only one version. By setting
    // public_key_version to 1, we make device policy requests succeed and thus
    // device policy timestamp set.
    device_state_.RequestDevicePolicyUpdate()
        ->policy_data()
        ->set_public_key_version(1);
  }

  bool ReadStatusFor(const std::string& policy_legend,
                     base::flat_map<std::string, std::string>* policy_status);
  bool ReloadPolicies();

 protected:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      ash::LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(),
      this,
      /*should_launch_browser=*/true,
      AccountId::FromUserEmailGaiaId(policy::PolicyBuilder::kFakeUsername,
                                     policy::PolicyBuilder::kFakeGaiaId)};
};

bool PolicyUIStatusTest::ReadStatusFor(
    const std::string& policy_legend,
    base::flat_map<std::string, std::string>* policy_status) {
  // Retrieve the text contents of the status table with specified legend.
  const std::string javascript = R"JS(
    (function() {
      function readStatus() {
        // Wait for the status box to appear in case page just loaded.
        const statusSection = document.getElementById('status-section');
        if (statusSection.hidden) {
          window.requestIdleCallback(readStatus);
          return;
        }

        const policies = getPolicyFieldsets();
        const statuses = {};
        for (let i = 0; i < policies.length; ++i) {
          const legend = policies[i].querySelector('legend').textContent;
          const entries = {};
          const rows = policies[i]
            .querySelectorAll('.status-entry div:nth-child(2)');
          for (let j = 0; j < rows.length; ++j) {
            entries[rows[j].className] = rows[j].textContent.trim();
          }
          statuses[legend.trim()] = entries;
        }
        domAutomationController.send(JSON.stringify(statuses));
      }
      window.requestIdleCallback(readStatus);
    })();
  )JS";
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  std::string json;
  if (!content::ExecuteScriptAndExtractString(contents, javascript, &json))
    return false;
  absl::optional<base::Value> statuses = base::JSONReader::Read(json);
  if (!statuses.has_value() || !statuses->is_dict())
    return false;
  const base::Value::Dict& status_dict = statuses->GetDict();
  const base::Value::Dict* actual_entries = status_dict.FindDict(policy_legend);
  if (!actual_entries) {
    return false;
  }
  for (const auto entry : *actual_entries) {
    policy_status->insert_or_assign(entry.first, entry.second.GetString());
  }
  return true;
}

bool PolicyUIStatusTest::ReloadPolicies() {
  const std::string javascript = R"JS(
    (function() {
      const reloadPoliciesBtn = document.getElementById('reload-policies');
      reloadPoliciesBtn.click();
      // Wait until reload button becomes enabled again, i.e. policies reloaded.
      function waitForPoliciesToReload() {
        if (reloadPoliciesBtn.disabled) {
          window.requestIdleCallback(waitForPoliciesToReload);
        } else {
          domAutomationController.send(true);
        }
      }
      window.requestIdleCallback(waitForPoliciesToReload);
    })();
  )JS";
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  bool ignored;
  return content::ExecuteScriptAndExtractBool(contents, javascript, &ignored);
}

IN_PROC_BROWSER_TEST_F(PolicyUIStatusTest,
                       ShowsZeroSecondsSinceRefreshAfterReloadingPolicies) {
  // Verifies that the time since refresh of a policy set is set to 0 seconds
  // after "Reload policies" button is pressed and policies are reloaded.

  // Mock time in policy server and classes used by refresh logic.
  base::Time now = base::Time::Now();
  logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
      ->UpdatePolicyTimestamp(now);
  base::SimpleTestClock status_provider_clock_mock;
  status_provider_clock_mock.SetNow(now);
  auto status_provider_clock_mock_closure =
      policy::PolicyStatusProvider::OverrideClockForTesting(
          &status_provider_clock_mock);
  base::SimpleTestClock policy_refresher_clock_mock;
  policy_refresher_clock_mock.SetNow(now);
  auto policy_refresher_clock_mock_closure =
      policy::CloudPolicyRefreshScheduler::OverrideClockForTesting(
          &policy_refresher_clock_mock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  ASSERT_TRUE(ReloadPolicies());

  base::flat_map<std::string, std::string> status;
  ASSERT_TRUE(ReadStatusFor("User policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "0 secs ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "0 secs ago");
  ASSERT_TRUE(ReadStatusFor("Device policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "0 secs ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "0 secs ago");
}

IN_PROC_BROWSER_TEST_F(PolicyUIStatusTest, ShowsCorrectTimesSinceRefresh) {
  // Verifies that the time since refresh of a policy set is correctly computed.

  // Mock time in policy server and classes used by refresh logic.
  base::Time now = base::Time::Now();
  logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
      ->UpdatePolicyTimestamp(now);
  base::SimpleTestClock status_provider_clock_mock;
  status_provider_clock_mock.SetNow(now);
  auto status_provider_clock_mock_closure =
      policy::PolicyStatusProvider::OverrideClockForTesting(
          &status_provider_clock_mock);
  base::SimpleTestClock policy_refresher_clock_mock;
  policy_refresher_clock_mock.SetNow(now);
  auto policy_refresher_clock_mock_closure =
      policy::CloudPolicyRefreshScheduler::OverrideClockForTesting(
          &policy_refresher_clock_mock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  ASSERT_TRUE(ReloadPolicies());
  status_provider_clock_mock.Advance(base::Hours(1));
  policy_refresher_clock_mock.Advance(base::Hours(1));
  // Refresh the page without reloading policies.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  base::RunLoop().RunUntilIdle();  // Ensure status request has been processed.

  base::flat_map<std::string, std::string> status;
  ASSERT_TRUE(ReadStatusFor("User policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "1 hour ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "1 hour ago");
  ASSERT_TRUE(ReadStatusFor("Device policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "1 hour ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "1 hour ago");
}

IN_PROC_BROWSER_TEST_F(PolicyUIStatusTest,
                       ShowsCorrectRefreshTimesAfterFailedReload) {
  // Verifies that the time since refresh of a policy set is correctly updated
  // after a failed attempt to update policies.

  // Mock time in policy server and classes used by refresh logic.
  base::Time now = base::Time::Now();
  logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
      ->UpdatePolicyTimestamp(now);
  base::SimpleTestClock status_provider_clock_mock;
  status_provider_clock_mock.SetNow(now);
  auto status_provider_clock_mock_closure =
      policy::PolicyStatusProvider::OverrideClockForTesting(
          &status_provider_clock_mock);
  base::SimpleTestClock policy_refresher_clock_mock;
  policy_refresher_clock_mock.SetNow(now);
  auto policy_refresher_clock_mock_closure =
      policy::CloudPolicyRefreshScheduler::OverrideClockForTesting(
          &policy_refresher_clock_mock);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  ASSERT_TRUE(ReloadPolicies());
  logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()->SetPolicyFetchError(
      500);
  status_provider_clock_mock.Advance(base::Hours(1));
  policy_refresher_clock_mock.Advance(base::Hours(1));
  ASSERT_TRUE(ReloadPolicies());

  base::flat_map<std::string, std::string> status;
  ASSERT_TRUE(ReadStatusFor("User policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "1 hour ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "0 secs ago");
  ASSERT_TRUE(ReadStatusFor("Device policies", &status));
  EXPECT_EQ(status["time-since-last-refresh"], "1 hour ago");
  EXPECT_EQ(status["time-since-last-fetch-attempt"], "0 secs ago");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PolicyUITest, SendPolicyNames) {
  // Verifies that the names of known policies are sent to the UI and processed
  // there correctly by checking that the policy table contains all policies in
  // the correct order.

  // Expect that the policy table contains all known policies in alphabetical
  // order and none of the policies have a set value.
  std::vector<std::vector<std::string>> expected_policies;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());
  for (policy::Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    expected_policies.push_back(PopulateExpectedPolicy(
        it.key(), std::string(), std::string(), nullptr, false));
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // Add policies found in the Policy Precedence table.
  for (auto* policy : policy::metapolicy::kPrecedence) {
    expected_policies.push_back(PopulateExpectedPolicy(
        policy, std::string(), std::string(), nullptr, false));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Retrieve the contents of the policy table from the UI and verify that it
  // matches the expectation.
  VerifyPolicies(expected_policies);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, SendPolicyValues) {
  // Verifies that policy values are sent to the UI and processed there
  // correctly by setting the values of four known and one unknown policy and
  // checking that the policy table contains the policy names, values and
  // metadata in the correct order.
  policy::PolicyMap values;
  std::map<std::string, std::string> expected_values;

  // Set the values of four existing policies.
  base::Value::List blocked_urls;
  blocked_urls.Append("site1.com");
  blocked_urls.Append("site2.com");
  blocked_urls.Append("site3.com");
  values.Set(policy::key::kURLBlocklist, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(blocked_urls)), nullptr);
  expected_values[policy::key::kURLBlocklist] =
      R"(["site1.com","site2.com","site3.com"])";
  values.Set(policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
             base::Value("http://google.com"), nullptr);
  expected_values[policy::key::kHomepageLocation] = "http://google.com";
  values.Set(policy::key::kSafeBrowsingProtectionLevel,
             policy::POLICY_LEVEL_RECOMMENDED, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  expected_values[policy::key::kSafeBrowsingProtectionLevel] = "1";
  values.Set(policy::key::kPasswordLeakDetectionEnabled,
             policy::POLICY_LEVEL_RECOMMENDED, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  expected_values[policy::key::kPasswordLeakDetectionEnabled] = "true";
  // Set the value of a policy that does not exist.
  const std::string kUnknownPolicy = "NoSuchThing";
  values.Set(kUnknownPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
             base::Value(true), nullptr);
  expected_values[kUnknownPolicy] = "true";
  const std::string kUnknownPolicyWithDots = "no.such.thing";
  values.Set(kUnknownPolicyWithDots, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
             base::Value("blub"), nullptr);
  expected_values[kUnknownPolicyWithDots] = "blub";

  provider_.UpdateChromePolicy(values);

  // Expect that the policy table contains, in order:
  // * All known policies whose value has been set, in alphabetical order.
  // * The unknown policy.
  // * All known policies whose value has not been set, in alphabetical order.
  std::vector<std::vector<std::string>> expected_policies;
  size_t first_unset_position = 0;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());
  for (policy::Schema::Iterator props = chrome_schema.GetPropertiesIterator();
       !props.IsAtEnd(); props.Advance()) {
    std::map<std::string, std::string>::const_iterator it =
        expected_values.find(props.key());
    const std::string value =
        it == expected_values.end() ? std::string() : it->second;
    const std::string source =
        it == expected_values.end() ? std::string() : "Cloud";
    const policy::PolicyMap::Entry* metadata = values.Get(props.key());
    expected_policies.insert(
        metadata ? expected_policies.begin() + first_unset_position++
                 : expected_policies.end(),
        PopulateExpectedPolicy(props.key(), value, source, metadata, false));
  }
  expected_policies.insert(
      expected_policies.begin() + first_unset_position++,
      PopulateExpectedPolicy(kUnknownPolicy, expected_values[kUnknownPolicy],
                             "Platform", values.Get(kUnknownPolicy), true));
  expected_policies.insert(
      expected_policies.begin() + first_unset_position++,
      PopulateExpectedPolicy(
          kUnknownPolicyWithDots, expected_values[kUnknownPolicyWithDots],
          "Platform", values.Get(kUnknownPolicyWithDots), true));

#if !BUILDFLAG(IS_CHROMEOS)
  // Add policies found in the Policy Precedence table.
  for (auto* policy : policy::metapolicy::kPrecedence) {
    expected_policies.push_back(PopulateExpectedPolicy(
        policy, std::string(), std::string(), values.Get(policy), false));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Retrieve the contents of the policy table from the UI and verify that it
  // matches the expectation.
  VerifyPolicies(expected_policies);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, ReportButton) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUIPolicyURL)));

  // Hide by default.
  VerifyReportButton(/*visible=*/false);

  // Turn on with the policy
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kCloudReportingEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  provider_.UpdateChromePolicy(policy_map);
#if !BUILDFLAG(IS_CHROMEOS)
  VerifyReportButton(/*visible=*/true);
#else
  // Always hide on Chrome OS.
  VerifyReportButton(/*visible=*/false);
#endif
  // Hide while policy is off.
  policy_map.Set(policy::key::kCloudReportingEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policy_map);
  VerifyReportButton(/*visible=*/false);
}

#if !BUILDFLAG(IS_CHROMEOS)
class PolicyPrecedenceUITest
    : public PolicyUITest,
      public ::testing::WithParamInterface<std::tuple<
          /*cloud_policy_overrides_platform_policy=*/bool,
          /*cloud_user_policy_overrides_cloud_machine_policy=*/bool,
          /*is_user_affiliated=*/bool>> {
 public:
  bool CloudPolicyOverridesPlatformPolicy() { return std::get<0>(GetParam()); }

  bool CloudUserPolicyOverridesCloudMachinePolicy() {
    return std::get<1>(GetParam());
  }

  bool IsUserAffiliated() { return std::get<2>(GetParam()); }

  void ValidatePrecedenceValue(const std::string& precedence_row_value) {
    if (CloudPolicyOverridesPlatformPolicy() &&
        CloudUserPolicyOverridesCloudMachinePolicy() && IsUserAffiliated()) {
      EXPECT_EQ(precedence_row_value,
                "Cloud user > Cloud machine > Platform machine > "
                "Platform user");
    } else if (CloudPolicyOverridesPlatformPolicy()) {
      EXPECT_EQ(precedence_row_value,
                "Cloud machine > Platform machine > Platform user > "
                "Cloud user");
    } else if (CloudUserPolicyOverridesCloudMachinePolicy() &&
               IsUserAffiliated()) {
      EXPECT_EQ(precedence_row_value,
                "Platform machine > Cloud user > Cloud machine > "
                "Platform user");
    } else {
      EXPECT_EQ(precedence_row_value,
                "Platform machine > Cloud machine > Platform user > "
                "Cloud user");
    }
  }

  // Used to retrieve the contents of the policy precedence rows.
  const std::string kJavaScript =
      "var precedence_row = getPrecedenceRowValue();"
      "domAutomationController.send(precedence_row.textContent);";
};

// Verify that the precedence order displayed in the Policy Precedence table is
// correct.
IN_PROC_BROWSER_TEST_P(PolicyPrecedenceUITest, PrecedenceOrder) {
  // Set precedence policies.
  policy::PolicyMap policy_map;

  if (IsUserAffiliated()) {
    base::flat_set<std::string> affiliation_ids;
    affiliation_ids.insert("12345");
    // Treat user as affiliated by setting identical user and device IDs.
    policy_map.SetUserAffiliationIds(affiliation_ids);
    policy_map.SetDeviceAffiliationIds(affiliation_ids);
  }

  policy_map.Set(policy::key::kCloudPolicyOverridesPlatformPolicy,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
  policy_map.Set(policy::key::kCloudUserPolicyOverridesCloudMachinePolicy,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(CloudUserPolicyOverridesCloudMachinePolicy()),
                 nullptr);
  provider_.UpdateChromePolicy(policy_map);

  // Retrieve the contents of the policy precedence rows.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUIPolicyURL)));
  std::string precedence_row_value;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), kJavaScript, &precedence_row_value));

  ValidatePrecedenceValue(precedence_row_value);
}

INSTANTIATE_TEST_SUITE_P(PolicyPrecedenceUITestInstance,
                         PolicyPrecedenceUITest,
                         testing::Combine(testing::Values(false, true),
                                          testing::Values(false, true),
                                          testing::Values(false, true)));
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// TODO(https://crbug.com/1027135) Add tests to verify extension policies are
// exported correctly.
class ExtensionPolicyUITest : public PolicyUITest,
                              public ::testing::WithParamInterface<bool> {
 public:
  ExtensionPolicyUITest() = default;

  bool UseSigninProfile() const { return GetParam(); }

  Profile* extension_profile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (UseSigninProfile()) {
      return ash::ProfileHelper::GetSigninProfile();
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return chrome_test_utils::GetProfile(this);
  }
};

// TODO(https://crbug.com/911661) Flaky time outs on Linux Chromium OS ASan
// LSan bot.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExtensionLoadAndSendPolicy DISABLED_ExtensionLoadAndSendPolicy
#else
#define MAYBE_ExtensionLoadAndSendPolicy ExtensionLoadAndSendPolicy
#endif
IN_PROC_BROWSER_TEST_P(ExtensionPolicyUITest,
                       MAYBE_ExtensionLoadAndSendPolicy) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  const std::string kNormalBooleanPolicy = "normal_boolean";
  const std::string kSensitiveBooleanPolicy = "sensitive_boolean";
  const std::string kSensitiveStringPolicy = "sensitive_string";
  const std::string kSensitiveObjectPolicy = "sensitive_object";
  const std::string kSensitiveArrayPolicy = "sensitive_array";
  const std::string kSensitiveIntegerPolicy = "sensitive_integer";
  const std::string kSensitiveNumberPolicy = "sensitive_number";
  std::string json_data = R"({
    "type": "object",
    "properties": {
      "normal_boolean": {
        "type": "boolean"
      },
      "sensitive_boolean": {
        "type": "boolean",
        "sensitiveValue": true
      },
      "sensitive_string": {
        "type": "string",
        "sensitiveValue": true
      },
      "sensitive_object": {
        "type": "object",
        "additionalProperties": {
          "type": "boolean"
        },
        "sensitiveValue": true
      },
      "sensitive_array": {
        "type": "array",
        "items": {
          "type": "boolean"
        },
        "sensitiveValue": true
      },
      "sensitive_integer": {
        "type": "integer",
        "sensitiveValue": true
      },
      "sensitive_number": {
        "type": "number",
        "sensitiveValue": true
      }
    }
  })";

  const std::string schema_file = "schema.json";
  base::FilePath schema_path = temp_dir_.GetPath().AppendASCII(schema_file);
  base::WriteFile(schema_path, json_data);

  // Build extension that contains the policy schema.
  extensions::DictionaryBuilder storage;
  storage.Set("managed_schema", schema_file);

  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "test")
      .Set("version", "1")
      .Set("manifest_version", 2)
      .Set("storage", storage.Build());

  extensions::ExtensionBuilder builder;
  builder.SetPath(temp_dir_.GetPath());
  builder.SetManifest(manifest.Build());
  builder.SetLocation(
      extensions::mojom::ManifestLocation::kExternalPolicyDownload);

  // Install extension.
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(extension_profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      extension->id());
  // Disable extension install verification.
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;

  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  policy::PolicyDomain policy_domain =
      UseSigninProfile() ? policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
                         : policy::POLICY_DOMAIN_EXTENSIONS;
  const policy::PolicyNamespace extension_policy_namespace(policy_domain,
                                                           extension->id());
  PolicySchemaAvailableWaiter(extension_profile()->GetOriginalProfile(),
                              extension_policy_namespace)
      .Wait();

  std::vector<std::vector<std::string>> expected_chrome_policies;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());

  for (policy::Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    expected_chrome_policies.push_back(PopulateExpectedPolicy(
        it.key(), std::string(), std::string(), nullptr, false));
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // Add policies found in the precedence policy table.
  for (auto* policy : policy::metapolicy::kPrecedence) {
    expected_chrome_policies.push_back(PopulateExpectedPolicy(
        policy, std::string(), std::string(), nullptr, false));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Add extension policy to expected policy list.
  std::vector<std::vector<std::string>> expected_policies =
      expected_chrome_policies;
  expected_policies.push_back(PopulateExpectedPolicy(
      kNormalBooleanPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveArrayPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveBooleanPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveIntegerPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveNumberPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveObjectPolicy, std::string(), std::string(), nullptr, false));
  expected_policies.push_back(PopulateExpectedPolicy(
      kSensitiveStringPolicy, std::string(), std::string(), nullptr, false));

  // Verify if policy UI includes policy that extension have.
  VerifyPolicies(expected_policies);

  base::Value::Dict object_value;
  object_value.Set("objectProperty", true);
  base::Value::List array_value;
  array_value.Append(true);

  policy::PolicyMap values;
  values.Set(kNormalBooleanPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(true), nullptr);
  values.Set(kSensitiveArrayPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(array_value)), nullptr);
  values.Set(kSensitiveBooleanPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(true), nullptr);
  values.Set(kSensitiveIntegerPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(42), nullptr);
  values.Set(kSensitiveNumberPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(3.141), nullptr);
  values.Set(kSensitiveObjectPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(object_value)), nullptr);
  values.Set(kSensitiveStringPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value("value"), nullptr);
  UpdateProviderPolicyForNamespace(extension_policy_namespace, values);

  // Add extension policy with values to expected policy list.
  const std::string mask_value = "********";
  std::vector<std::vector<std::string>> expected_policies_with_values =
      expected_chrome_policies;
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kNormalBooleanPolicy, "true", "Cloud",
                             values.Get(kNormalBooleanPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveArrayPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveArrayPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveBooleanPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveBooleanPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveIntegerPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveIntegerPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveNumberPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveNumberPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveObjectPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveObjectPolicy), false));
  expected_policies_with_values.push_back(
      PopulateExpectedPolicy(kSensitiveStringPolicy, mask_value, "Cloud",
                             values.Get(kSensitiveStringPolicy), false));
  VerifyPolicies(expected_policies_with_values);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionPolicyUITest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                         ::testing::Values(false, true)
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
                         ::testing::Values(false)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
);

#endif  // !BUILDFLAG(IS_ANDROID)
