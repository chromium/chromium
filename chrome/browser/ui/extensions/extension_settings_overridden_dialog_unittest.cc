// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/search_override_stack.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"

namespace {

using DialogResult = SettingsOverriddenDialogController::DialogResult;

constexpr char kTestAcknowledgedPreference[] = "TestPreference";
constexpr char kTestDialogResultHistogramName[] = "TestHistogramName";
constexpr char kStackStateHistogramName[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogStackState";
constexpr char kChainLengthHistogramName[] =
    "Extensions.SettingsOverridden."
    "SearchOverriddenDialogUnacknowledgedChainLength";
constexpr char kDialogResultForChainOverDefaultHistogramName[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogResult."
    "UnacknowledgedChainOverNonExtensionDefault";

ExtensionSettingsOverriddenDialog::Params CreateTestDialogParams(
    const extensions::ExtensionId& controlling_id) {
  SettingsOverriddenDialogController::ShowParams show_params(
      u"Test Dialog Title", u"Test Dialog Body", nullptr);
  return {controlling_id, "Test Extension", kTestAcknowledgedPreference,
          kTestDialogResultHistogramName, std::move(show_params)};
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
    if (include_extra_perms) {
      builder.AddAPIPermission("storage");
    }
    scoped_refptr<const extensions::Extension> extension = builder.Build();
    registrar()->AddExtension(extension);
    SetExtensionInstallTime(extension->id(), base::Time::Now());
    return extension.get();
  }

  // Updates the install time for a specific extension to a specific time.
  void SetExtensionInstallTime(const extensions::ExtensionId& id,
                               base::Time time) {
    extensions::ExtensionPrefs::Get(profile())->UpdateExtensionPref(
        id, extensions::kPrefFirstInstallTime,
        base::Value(base::NumberToString(
            time.ToDeltaSinceWindowsEpoch().InMicroseconds())));
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
      CreateTestDialogParams(extension->id()), *profile());
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
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_FALSE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       WontShowForAnExtensionThatCantBeDisabled) {
  const extensions::Extension* policy_extension = AddExtension(
      "policy installed",
      extensions::mojom::ManifestLocation::kExternalPolicyDownload);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(policy_extension->id()), *profile());
  EXPECT_FALSE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionDisabledOnDialogRejection) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogWillBeShown();

  controller.HandleDialogResult(DialogResult::kChangeSettingsBack);
  histogram_tester.ExpectUniqueSample(kTestDialogResultHistogramName,
                                      DialogResult::kChangeSettingsBack, 1);

  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  EXPECT_THAT(GetExtensionPrefs()->GetDisableReasons(extension->id()),
              testing::UnorderedElementsAre(
                  extensions::disable_reason::DISABLE_USER_ACTION));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionAcknowledgedOnDialogAcceptance) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogWillBeShown();

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
      CreateTestDialogParams(extension->id()), *profile());
  controller.OnDialogWillBeShown();

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
      CreateTestDialogParams(extension->id()), *profile());
  controller.OnDialogWillBeShown();

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
        CreateTestDialogParams(extension->id()), *profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogWillBeShown();
    controller.HandleDialogResult(DialogResult::kDialogDismissed);
  }

  {
    // Since the dialog was already shown for this extension, it should not
    // display a second time.
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension->id()), *profile());
    EXPECT_FALSE(controller.ShouldShow());
  }
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       CanShowForDifferentExtensionsInTheSameSession) {
  const extensions::Extension* extension_one = AddExtension("one");

  {
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension_one->id()), *profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogWillBeShown();
    controller.HandleDialogResult(DialogResult::kDialogDismissed);
  }

  const extensions::Extension* extension_two = AddExtension("two");
  {
    ExtensionSettingsOverriddenDialog controller(
        CreateTestDialogParams(extension_two->id()), *profile());
    EXPECT_TRUE(controller.ShouldShow());
  }
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       ExtensionRemovedWhileDialogShown) {
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogWillBeShown();

  registrar()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  controller.HandleDialogResult(DialogResult::kChangeSettingsBack);
}

// Tests that simple override extensions don't trigger the settings overridden
// dialog.
TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       SimpleOverrideExtensionDoesntTriggerDialog) {
  const extensions::Extension* extension =
      AddExtension("alpha", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/false);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_FALSE(controller.ShouldShow());
  // The the extension should not be acknowledged. The latter is important to
  // re-assess the extension in case it updates.
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       SimpleOverrideNewInstallationTriggersDialog) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineUnconditionalDialog);

  // 1. Set the enforcement timestamp to the past.
  profile()->GetPrefs()->SetTime(ExtensionSettingsOverriddenDialog::
                                     kSimpleOverrideBeginConfirmationTimestamp,
                                 base::Time::Now() - base::Days(1));

  // 2. Install a simple override extension. Its install time will be "Now",
  // which is later than the enforcement timestamp.
  const extensions::Extension* extension =
      AddExtension("simple_new", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/false);

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());

  // Since InstallTime > EnforcementTime, it should show.
  EXPECT_TRUE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       SimpleOverrideOldInstallationGrandfathered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineUnconditionalDialog);

  // 1. Install a simple override extension. Its install time is "Now".
  const extensions::Extension* extension =
      AddExtension("simple_old", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/false);

  // 2. Set the enforcement timestamp to the future.
  profile()->GetPrefs()->SetTime(ExtensionSettingsOverriddenDialog::
                                     kSimpleOverrideBeginConfirmationTimestamp,
                                 base::Time::Now() + base::Days(1));

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());

  // Since InstallTime < EnforcementTime, it should NOT show.
  EXPECT_FALSE(controller.ShouldShow());
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       SimpleOverrideFirstRunCreatesPrefAndGrandfathers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineUnconditionalDialog);

  // 1. Install a simple override extension.
  const extensions::Extension* extension = AddExtension(
      "simple_first_run", extensions::mojom::ManifestLocation::kInternal,
      /*include_extra_perms=*/false);

  // Set the install time to the past to ensure it is strictly before the
  // "Now" that will be generated inside ShouldShow().
  SetExtensionInstallTime(extension->id(),
                          base::Time::Now() - base::Seconds(10));

  // 2. Ensure the preference does not exist yet.
  PrefService* prefs = profile()->GetPrefs();
  EXPECT_TRUE(prefs
                  ->GetTime(ExtensionSettingsOverriddenDialog::
                                kSimpleOverrideBeginConfirmationTimestamp)
                  .is_null());

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());

  // 3. It should not show (Grandfathered), because InstallTime <
  // EnforcementTime (Now).
  EXPECT_FALSE(controller.ShouldShow());

  // 4. The preference should have been created and set to the current time.
  EXPECT_FALSE(prefs
                   ->GetTime(ExtensionSettingsOverriddenDialog::
                                 kSimpleOverrideBeginConfirmationTimestamp)
                   .is_null());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       NonSimpleOverrideAlwaysTriggersIgnoresTimestamp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineUnconditionalDialog);

  const extensions::Extension* extension =
      AddExtension("complex", extensions::mojom::ManifestLocation::kInternal,
                   /*include_extra_perms=*/true);

  // Set the enforcement timestamp to the future. If this were a simple
  // override, it would be grandfathered. However, for non-simple overrides,
  // this pref should be irrelevant.
  profile()->GetPrefs()->SetTime(ExtensionSettingsOverriddenDialog::
                                     kSimpleOverrideBeginConfirmationTimestamp,
                                 base::Time::Now() + base::Days(1));

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());

  EXPECT_TRUE(controller.ShouldShow());
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       RecordsSearchOverrideStackOncePerExtensionWhenShown) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  // A search dialog whose "previous choice" is an extension the user never
  // acknowledged, re-shown on every search until they choose.
  auto create_params = [&]() {
    ExtensionSettingsOverriddenDialog::Params params =
        CreateTestDialogParams(extension->id());
    params.unlimited_shows = true;
    params.search_override_stack.emplace();
    params.search_override_stack->state = extensions::SearchOverrideStackState::
        kUnacknowledgedChainOverNonExtensionDefault;
    params.search_override_stack->unacknowledged_chain_length = 2;
    return params;
  };

  {
    ExtensionSettingsOverriddenDialog controller(create_params(), *profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogWillBeShown();
    controller.HandleDialogResult(DialogResult::kDialogDismissed);
  }

  histogram_tester.ExpectUniqueSample(
      kStackStateHistogramName,
      extensions::SearchOverrideStackState::
          kUnacknowledgedChainOverNonExtensionDefault,
      1);
  histogram_tester.ExpectUniqueSample(kChainLengthHistogramName, 2, 1);
  histogram_tester.ExpectUniqueSample(
      kDialogResultForChainOverDefaultHistogramName,
      DialogResult::kDialogDismissed, 1);

  // The stack is recorded once; the result every time.
  {
    ExtensionSettingsOverriddenDialog controller(create_params(), *profile());
    EXPECT_TRUE(controller.ShouldShow());
    controller.OnDialogWillBeShown();
    controller.HandleDialogResult(DialogResult::kKeepNewSettings);
  }

  histogram_tester.ExpectTotalCount(kStackStateHistogramName, 1);
  histogram_tester.ExpectTotalCount(kChainLengthHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kDialogResultForChainOverDefaultHistogramName,
      DialogResult::kKeepNewSettings, 1);
  histogram_tester.ExpectTotalCount(
      kDialogResultForChainOverDefaultHistogramName, 2);
}

TEST_F(ExtensionSettingsOverriddenDialogUnitTest,
       DoesNotRecordSearchOverrideStackWithoutOne) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtension();

  ExtensionSettingsOverriddenDialog controller(
      CreateTestDialogParams(extension->id()), *profile());
  EXPECT_TRUE(controller.ShouldShow());
  controller.OnDialogWillBeShown();
  controller.HandleDialogResult(DialogResult::kKeepNewSettings);

  histogram_tester.ExpectTotalCount(kStackStateHistogramName, 0);
  histogram_tester.ExpectTotalCount(kChainLengthHistogramName, 0);
  EXPECT_TRUE(histogram_tester
                  .GetTotalCountsForPrefix("Extensions.SettingsOverridden."
                                           "SearchOverriddenDialogResult.")
                  .empty());
}
