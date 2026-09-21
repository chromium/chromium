// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/search_override_stack.h"

#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "components/search_engines/default_search_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

constexpr char kStackStateHistogramName[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogStackState";
constexpr char kChainLengthHistogramName[] =
    "Extensions.SettingsOverridden."
    "SearchOverriddenDialogUnacknowledgedChainLength";
constexpr char kDialogResultHistogramPrefix[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogResult.";
constexpr char kProfileStackStateHistogramName[] =
    "Extensions.SettingsOverridden.DseExtensionStackState";

using DialogResult = SettingsOverriddenDialogController::DialogResult;

constexpr char kExtensionIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kExtensionIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

// Later `order` takes precedence.
base::Time InstallTime(int order) {
  return base::Time::UnixEpoch() + base::Days(order);
}

}  // namespace

class SearchOverrideStackTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  // Sets the default search preference the way installing an extension with a
  // default search_provider does in production.
  const Extension* AddSearchOverridingExtension(
      const std::string& name,
      base::Time install_time,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(name).SetLocation(location).Build();
    ExtensionPrefValueMap* value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile());
    value_map->RegisterExtension(extension->id(), install_time,
                                 /*is_enabled=*/true,
                                 /*is_incognito_enabled=*/false);
    value_map->SetExtensionPref(
        extension->id(),
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        ExtensionPrefValueMap::ChromeSettingScope::kRegular,
        base::Value(base::DictValue()));
    registrar()->AddExtension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
    return extension.get();
  }

  // Marks the extension accepted in the dialog.
  void AcknowledgeExtension(const ExtensionId& id) {
    ExtensionPrefs::Get(profile())->UpdateExtensionPref(
        id, ControlledHomeDialogController::kAcknowledgedPreference,
        base::Value(true));
  }
};

TEST_F(SearchOverrideStackTest, NoOverridingExtension) {
  EXPECT_EQ(std::nullopt,
            GetSearchOverrideStackInfo(*profile(), kExtensionIdA));
}

TEST_F(SearchOverrideStackTest, SingleOverride) {
  const Extension* extension =
      AddSearchOverridingExtension("only", InstallTime(1));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::kSingleOverride, info->state);
  EXPECT_EQ(0, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, UnacknowledgedChainOverNonExtensionDefault) {
  AddSearchOverridingExtension("a", InstallTime(1));
  AddSearchOverridingExtension("b", InstallTime(2));
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(3));
  // The user's own default is buried under two extensions they never saw.
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(
      SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault,
      info->state);
  EXPECT_EQ(2, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, AcknowledgedExtensionNext) {
  const Extension* extension_a =
      AddSearchOverridingExtension("a", InstallTime(1));
  AcknowledgeExtension(extension_a->id());
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(2));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::kAcknowledgedExtensionNext, info->state);
  EXPECT_EQ(0, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, UnacknowledgedChainOverAcknowledgedExtension) {
  // Reverting stops at the acknowledged extension, so what's below it isn't
  // part of the chain.
  AddSearchOverridingExtension("z", InstallTime(1));
  const Extension* extension_a =
      AddSearchOverridingExtension("a", InstallTime(2));
  AcknowledgeExtension(extension_a->id());
  AddSearchOverridingExtension("b", InstallTime(3));
  AddSearchOverridingExtension("c", InstallTime(4));
  const Extension* extension_d =
      AddSearchOverridingExtension("d", InstallTime(5));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_d->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(
      SearchOverrideStackState::kUnacknowledgedChainOverAcknowledgedExtension,
      info->state);
  EXPECT_EQ(2, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, MustRemainEnabledExtensionNext) {
  // Can't be disabled, but the user never accepted it either.
  AddSearchOverridingExtension(
      "policy", InstallTime(1),
      mojom::ManifestLocation::kExternalPolicyDownload);
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(2));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::kMustRemainEnabledExtensionNext,
            info->state);
  EXPECT_EQ(0, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, MustRemainEnabledExtensionEndsTheChain) {
  AddSearchOverridingExtension(
      "policy", InstallTime(1),
      mojom::ManifestLocation::kExternalPolicyDownload);
  AddSearchOverridingExtension("b", InstallTime(2));
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(3));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::
                kUnacknowledgedChainOverMustRemainEnabledExtension,
            info->state);
  EXPECT_EQ(1, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest,
       AcknowledgedMustRemainEnabledExtensionCountsAsAcknowledged) {
  const Extension* policy_extension = AddSearchOverridingExtension(
      "policy", InstallTime(1),
      mojom::ManifestLocation::kExternalPolicyDownload);
  AcknowledgeExtension(policy_extension->id());
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(2));
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::kAcknowledgedExtensionNext, info->state);
}

TEST_F(SearchOverrideStackTest, DisabledExtensionsAreNotInTheStack) {
  const Extension* extension_a =
      AddSearchOverridingExtension("a", InstallTime(1));
  const Extension* extension_b =
      AddSearchOverridingExtension("b", InstallTime(2));
  const Extension* extension_c =
      AddSearchOverridingExtension("c", InstallTime(3));
  registrar()->DisableExtension(extension_b->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(
      SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault,
      info->state);
  EXPECT_EQ(1, info->unacknowledged_chain_length);
  registrar()->DisableExtension(extension_a->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  info = GetSearchOverrideStackInfo(*profile(), extension_c->id());
  ASSERT_TRUE(info);
  EXPECT_EQ(SearchOverrideStackState::kSingleOverride, info->state);
  EXPECT_EQ(0, info->unacknowledged_chain_length);
}

TEST_F(SearchOverrideStackTest, NoInfoWhenExtensionIsNotInControl) {
  const Extension* extension_a =
      AddSearchOverridingExtension("a", InstallTime(1));
  AddSearchOverridingExtension("c", InstallTime(2));
  EXPECT_EQ(std::nullopt,
            GetSearchOverrideStackInfo(*profile(), extension_a->id()));
}

TEST_F(SearchOverrideStackTest, RecordsMetricsOncePerControllingExtension) {
  base::HistogramTester histogram_tester;
  SearchOverrideStackInfo info;
  info.state =
      SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault;
  info.unacknowledged_chain_length = 2;
  RecordSearchOverrideStackMetricsOnce(*profile(), kExtensionIdA, info);
  RecordSearchOverrideStackMetricsOnce(*profile(), kExtensionIdA, info);
  histogram_tester.ExpectUniqueSample(
      kStackStateHistogramName,
      SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault, 1);
  histogram_tester.ExpectUniqueSample(kChainLengthHistogramName, 2, 1);
  // A different controlling extension records separately.
  RecordSearchOverrideStackMetricsOnce(*profile(), kExtensionIdB,
                                       SearchOverrideStackInfo());
  histogram_tester.ExpectBucketCount(
      kStackStateHistogramName, SearchOverrideStackState::kSingleOverride, 1);
  histogram_tester.ExpectBucketCount(kChainLengthHistogramName, 0, 1);
  histogram_tester.ExpectTotalCount(kStackStateHistogramName, 2);
  histogram_tester.ExpectTotalCount(kChainLengthHistogramName, 2);
}

TEST_F(SearchOverrideStackTest, RecordsDialogResultByStackState) {
  base::HistogramTester histogram_tester;
  SearchOverrideStackInfo info;
  info.state =
      SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault;
  RecordSearchOverrideStackDialogResult(info,
                                        DialogResult::kChangeSettingsBack);
  RecordSearchOverrideStackDialogResult(info,
                                        DialogResult::kChangeSettingsBack);
  RecordSearchOverrideStackDialogResult(info, DialogResult::kDialogDismissed);
  // Not deduplicated: every choice counts.
  const std::string histogram_name =
      base::StrCat({kDialogResultHistogramPrefix,
                    "UnacknowledgedChainOverNonExtensionDefault"});
  histogram_tester.ExpectBucketCount(histogram_name,
                                     DialogResult::kChangeSettingsBack, 2);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     DialogResult::kDialogDismissed, 1);
  histogram_tester.ExpectTotalCount(histogram_name, 3);
}

// Every state maps to its own histogram variant.
TEST_F(SearchOverrideStackTest, DialogResultVariantPerStackState) {
  base::HistogramTester histogram_tester;
  constexpr auto kVariants = std::to_array<std::string_view>({
      "SingleOverride",
      "AcknowledgedExtensionNext",
      "UnacknowledgedChainOverAcknowledgedExtension",
      "UnacknowledgedChainOverNonExtensionDefault",
      "MustRemainEnabledExtensionNext",
      "UnacknowledgedChainOverMustRemainEnabledExtension",
  });
  static_assert(std::size(kVariants) ==
                static_cast<size_t>(SearchOverrideStackState::kMaxValue) + 1);
  for (size_t i = 0; i < std::size(kVariants); ++i) {
    SearchOverrideStackInfo info;
    info.state = static_cast<SearchOverrideStackState>(i);
    RecordSearchOverrideStackDialogResult(info, DialogResult::kKeepNewSettings);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({kDialogResultHistogramPrefix, kVariants[i]}),
        DialogResult::kKeepNewSettings, 1);
  }
}

TEST_F(SearchOverrideStackTest,
       DoesNotRecordProfileStackStateWithoutOverridingExtension) {
  base::HistogramTester histogram_tester;
  RecordDseExtensionStackStateOnce(*profile());
  histogram_tester.ExpectTotalCount(kProfileStackStateHistogramName, 0);
}

}  // namespace extensions
