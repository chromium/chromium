// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_extensions_handler.h"

#include <string>

#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using mojom::ManifestLocation;

namespace {

const char kAllHostsPermission[] = "*://*/*";

// These `cws_info` variables are used to test the various
// `safety_check_extensions_handler` states.
// Will trigger the extension handler due to the malware violation.
static extensions::CWSInfoService::CWSInfo cws_info_malware{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    false,
    false};
// Will trigger the extension handler due to the policy violation.
static extensions::CWSInfoService::CWSInfo cws_info_policy{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kPolicy,
    false,
    false};
// Will trigger the extension handler due to being unpublished.
static extensions::CWSInfoService::CWSInfo cws_info_unpublished{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kNone,
    true,
    false};
// Will trigger the extension handler due to multiple triggers.
static extensions::CWSInfoService::CWSInfo cws_info_multi{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    true,
    false};
// Will not trigger the extension handler.
static extensions::CWSInfoService::CWSInfo cws_info_no_trigger{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kNone,
    false,
    false};
// Will not trigger the extension handler.
static extensions::CWSInfoService::CWSInfo cws_info_no_data{
    false,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    false,
    false};

class MockCWSInfoService : public extensions::CWSInfoService {
 public:
  MOCK_METHOD(absl::optional<bool>,
              IsLiveInCWS,
              (const extensions::Extension&),
              (const, override));
  MOCK_METHOD(absl::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const extensions::Extension&),
              (const, override));
  MOCK_METHOD(void, CheckAndMaybeFetchInfo, (), (override));
  MOCK_METHOD(void,
              AddObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));
};

}  // namespace

class SafetyCheckExtensionsHandlerTest : public testing::Test {
 public:
  void SetUp() override;

  int GetNumberOfExtensionsThatNeedReview() {
    return safety_check_handler_->GetNumberOfExtensionsThatNeedReview();
  }

 protected:
  void AddExtension(const std::string& name, mojom::ManifestLocation location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension = CreateExtension(name, location);
    extensions::ExtensionRegistry::Get(profile_.get())->AddEnabled(extension);
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& name,
      mojom::ManifestLocation location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const extensions::Extension> extension =
        ExtensionBuilder(name)
            .SetManifestKey("host_permissions",
                            base::Value::List().Append(kAllHostsPermission))
            .SetLocation(location)
            .SetID(kId)
            .Build();
    extensions::ExtensionPrefs::Get(profile_.get())
        ->OnExtensionInstalled(extension.get(),
                               extensions::Extension::State::ENABLED,
                               syncer::StringOrdinal(), "");
    return extension;
  }

  void RemoveExtension(const Extension* extension) {
    extensions::ExtensionRegistry::Get(profile_.get())
        ->TriggerOnUninstalled(extension,
                               extensions::UNINSTALL_REASON_FOR_TESTING);
  }

  void AcknowledgeSafetyCheck(const std::string& name) {
    extensions::ExtensionPrefs::Get(profile_.get())
        ->UpdateExtensionPref(name, "ack_safety_check_warning",
                              base::Value(true));
  }

  content::TestWebUI web_ui_;
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service_;
  std::unique_ptr<settings::SafetyCheckExtensionsHandler> safety_check_handler_;
};

void SafetyCheckExtensionsHandlerTest::SetUp() {
  TestingProfile::Builder builder;
  profile_ = builder.Build();
  safety_check_handler_ =
      std::make_unique<settings::SafetyCheckExtensionsHandler>(profile_.get());
  safety_check_handler_->SetCWSInfoServiceForTest(&mock_cws_info_service_);
  safety_check_handler_.get()->set_web_ui(&web_ui_);
  safety_check_handler_.get()->AllowJavascript();
}

TEST_F(SafetyCheckExtensionsHandlerTest,
       GetNumberOfExtensionsThatNeedReviewTest) {
  // Create fake extensions for our pref service to load.
  AddExtension("TestExtension1", ManifestLocation::kInternal);
  AddExtension("TestExtension2", ManifestLocation::kInternal);
  AddExtension("TestExtension3", ManifestLocation::kInternal);
  AddExtension("TestExtension4", ManifestLocation::kInternal);
  AddExtension("TestExtension5", ManifestLocation::kInternal);
  AddExtension("TestExtension6", ManifestLocation::kInternal);
  // Extensions installed by policies will be ignored by the safety
  // check. So extension 7 will not trigger the handler.
  AddExtension("TestExtension7", ManifestLocation::kExternalPolicyDownload);
  // Ensure that the mock CWSInfo service returns the needed information.
  EXPECT_CALL(mock_cws_info_service_, GetCWSInfo)
      .Times(6)
      .WillOnce(testing::Return(cws_info_malware))
      .WillOnce(testing::Return(cws_info_policy))
      .WillOnce(testing::Return(cws_info_unpublished))
      .WillOnce(testing::Return(cws_info_multi))
      .WillOnce(testing::Return(cws_info_no_data))
      .WillOnce(testing::Return(cws_info_no_trigger));
  // There should be 4 triggering extensions based on the various cws_info
  // variables.
  EXPECT_EQ(4, GetNumberOfExtensionsThatNeedReview());
}

TEST_F(SafetyCheckExtensionsHandlerTest, OnExtensionPrefsDeletedTest) {
  // Create fake extensions for our pref service to load and test that
  // the `web_ui_` has recorded no events
  AddExtension("BadTestExtension1", ManifestLocation::kInternal);
  auto bad_test_extension2 =
      CreateExtension("BadTestExtension2", ManifestLocation::kInternal);
  AddExtension("GoodTestExtension1", ManifestLocation::kInternal);
  auto good_test_extension2 =
      CreateExtension("GoodTestExtension2", ManifestLocation::kInternal);
  safety_check_handler_.get()->SetTriggeringExtensionsForTest(
      crx_file::id_util::GenerateId("BadTestExtension1"));
  safety_check_handler_.get()->SetTriggeringExtensionsForTest(
      crx_file::id_util::GenerateId("BadTestExtension2"));
  EXPECT_EQ(0u, web_ui_.call_data().size());
  // After `AcknowledgeSafetyCheck` one event should have been fired.
  AcknowledgeSafetyCheck(crx_file::id_util::GenerateId("BadTestExtension1"));
  EXPECT_EQ(1u, web_ui_.call_data().size());
  // After `RemoveExtension` one additional event should have been fired
  // making two total events.
  RemoveExtension(bad_test_extension2.get());
  EXPECT_EQ(2u, web_ui_.call_data().size());

  // When the same actions are preformed on good extensions, no more
  // events are fired.
  AcknowledgeSafetyCheck(crx_file::id_util::GenerateId("GoodTestExtension1"));
  EXPECT_EQ(2u, web_ui_.call_data().size());
  RemoveExtension(good_test_extension2.get());
  EXPECT_EQ(2u, web_ui_.call_data().size());
}
}  // namespace extensions
