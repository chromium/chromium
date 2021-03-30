// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"

namespace {

using DialogResult = SettingsOverriddenDialogController::DialogResult;

constexpr char kTestAcknowledgedPreference[] = "TestPreference";
constexpr char kTestDialogResultHistogramName[] = "TestHistogramName";

ExtensionSettingsOverriddenDialog::Params CreateTestDialogParams(
    const extensions::ExtensionId& controlling_id) {
  return {controlling_id,
          kTestAcknowledgedPreference,
          kTestDialogResultHistogramName,
          u"Test Dialog Title",
          u"Test Dialog Body",
          nullptr};
}

}  // namespace

class ExtensionSettingsOverriddenDialogUnitTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  // Adds a new extension with the given |name| and |location| to the profile.
  const extensions::Extension* AddExtension(
      const char* name = "alpha",
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name).SetLocation(location).Build();
    service()->AddExtension(extension.get());

    // Make sure RegisterClient calls for storage are finished to avoid flaky
    // crashes in QuotaManagerImpl::RegisterClient.
    // TODO(crbug.com/1182630) : Remove this when 1182630 is fixed.
    extensions::util::GetStoragePartitionForExtensionId(extension->id(),
                                                        profile());
    task_environment()->RunUntilIdle();
    return extension.get();
  }

  extensions::ExtensionPrefs* GetExtensionPrefs() {
    return extensions::ExtensionPrefs::Get(profile());
  }

  // Returns true if the extension with the given |id| has been marked as
  // acknowledged.
  bool IsExtensionAcknowledged(const extensions::ExtensionId& id) {
    bool acknowledged = false;
    return GetExtensionPrefs()->ReadPrefAsBoolean(
               id, kTestAcknowledgedPreference, &acknowledged) &&
           acknowledged;
  }
};

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ShouldShowWithAControllingExtension) {
  const extensions::Extension* extension = AddExtension("fancy extension");

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  EXPECT_TRUE(controller.ShouldShow());

  ExtensionSettingsOverriddenDialog::ShowParams show_params =
      controller.GetShowParams();
  EXPECT_EQ("Test Dialog Title", base::UTF16ToUTF8(show_params.dialog_title));
  EXPECT_EQ("Test Dialog Body", base::UTF16ToUTF8(show_params.message));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       WontShowForAnAcknowledgedExtension) {
  const extensions::Extension* extension = AddExtension();
  GetExtensionPrefs()->UpdateExtensionPref(extension->id(),
                                           kTestAcknowledgedPreference,
                                           std::make_unique<base::Value>(true));

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  EXPECT_FALSE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       WontShowForAnExtensionThatCantBeDisabled) {
  const extensions::Extension* policy_extension = AddExtension(
      "policy installed",
      extensions::mojom::ManifestLocation::kExternalPolicyDownload);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(policy_extension->id()), profile());
  EXPECT_FALSE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionDisabledOnDialogRejection) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogShown();

  controller.HandleDialogResult(DialogResult::kChangeSettingsBack);
  histogram_tester.ExpectUniqueSample(kTestDialogResultHistogramName,
                                      DialogResult::kChangeSettingsBack, 1);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  EXPECT_EQ(extensions::disable_reason::DISABLE_USER_ACTION,
            GetExtensionPrefs()->GetDisableReasons(extension->id()));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionAcknowledgedOnDialogAcceptance) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogShown();

  controller.HandleDialogResult(DialogResult::kKeepNewSettings);
  histogram_tester.ExpectUniqueSample(kTestDialogResultHistogramName,
                                      DialogResult::kKeepNewSettings, 1);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_TRUE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionIsNeitherDisabledNorAcknowledgedOnDialogDismissal) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  controller.OnDialogShown();

  controller.HandleDialogResult(DialogResult::kDialogDismissed);
  histogram_tester.ExpectUniqueSample(kTestDialogResultHistogramName,
                                      DialogResult::kDialogDismissed, 1);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(
    ExtensionSettingsOverriddenDialogUnitTest,
    ExtensionIsNeitherDisabledNorAcknowledgedOnDialogCloseWithoutUserAction) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  controller.OnDialogShown();

  controller.HandleDialogResult(DialogResult::kDialogClosedWithoutUserAction);
  histogram_tester.ExpectUniqueSample(
      kTestDialogResultHistogramName,
      DialogResult::kDialogClosedWithoutUserAction, 1);

  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       WontShowTwiceForTheSameExtensionInTheSameSession) {
  const extensions::Extension* extension = AddExtension();

  {
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension->id()), profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogShown();
    controller.HandleDialogResult(DialogResult::kDialogDismissed);
  }

  {
    // Since the dialog was already shown for this extension, it should not
    // display a second time.
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension->id()), profile());
    EXPECT_FALSE(controller.ShouldShow());
  }
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       CanShowForDifferentExtensionsInTheSameSession) {
  const extensions::Extension* extension_one = AddExtension("one");

  {
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension_one->id()), profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogShown();
    controller.HandleDialogResult(DialogResult::kDialogDismissed);
  }

  const extensions::Extension* extension_two = AddExtension("two");
  {
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension_two->id()), profile());
    EXPECT_TRUE(controller.ShouldShow());
  }
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionRemovedWhileDialogShown) {
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogShown();

  service()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  controller.HandleDialogResult(DialogResult::kChangeSettingsBack);
}
