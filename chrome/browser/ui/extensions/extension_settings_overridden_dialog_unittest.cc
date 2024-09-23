// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"

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

  // Adds a new extension with the given `name` and `location` to the profile.
  // If `include_extra_perms` is true, this also adds a simple permission to
  // the extension (so that it's not considered a "simple override").
  const extensions::Extension* AddExtension(
      const char* name = "alpha",
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal,
      bool include_extra_perms = true) {
    extensions::ExtensionBuilder builder(name);
    builder.SetLocation(location);
    if (include_extra_perms)
      builder.AddAPIPermission("storage");
    scoped_refptr<const extensions::Extension> extension = builder.Build();
    service()->AddExtension(extension.get());
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
  EXPECT_EQ(u"Test Dialog Title", show_params.dialog_title);
  EXPECT_EQ(u"Test Dialog Body", show_params.message);
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       WontShowForAnAcknowledgedExtension) {
  const extensions::Extension* extension = AddExtension();
  GetExtensionPrefs()->UpdateExtensionPref(
      extension->id(), kTestAcknowledgedPreference, base::Value(true));

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

class LightweightExtensionSettingsOverriddenDialogTest
    : public ExtensionSettingsOverriddenDialogUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  LightweightExtensionSettingsOverriddenDialogTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kLightweightExtensionOverrideConfirmations);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kLightweightExtensionOverrideConfirmations);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LightweightExtensionSettingsOverriddenDialogTest,
                         testing::Bool());

// Tests that simple override extensions don't trigger the settings overridden
// dialog if the lightweight extension overrides experiment is enabled.
TEST_P(LightweightExtensionSettingsOverriddenDialogTest,
       SimpleOverrideExtensionDoesntTriggerDialog) {
  const extensions::Extension* extension =
      AddExtension("alpha", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/false);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  // The dialog should *not* want to show if the feature is enabled.
  bool expect_should_show = !GetParam();
  EXPECT_EQ(expect_should_show, controller.ShouldShow());
  // Regardless of features enablement, the the extension should not be
  // acknowledged. The latter is important to re-assess the extension in case
  // it updates.
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

// Tests that simple override extensions don't trigger the settings overridden
// dialog if the lightweight extension overrides experiment is enabled.
TEST_P(LightweightExtensionSettingsOverriddenDialogTest,
       NonSimpleOverrideExtensionAlwaysTriggersDialog) {
  const extensions::Extension* extension =
      AddExtension("alpha", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/true);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), profile());
  // The dialog should always show, regardless of feature state.
  EXPECT_TRUE(controller.ShouldShow());
}
