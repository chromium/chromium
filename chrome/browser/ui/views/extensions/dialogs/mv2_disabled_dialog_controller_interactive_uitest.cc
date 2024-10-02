// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/wm/window_restore/window_restore_util.h"
#endif

namespace extensions {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

bool HasExtensionByName(std::string_view name, const ExtensionSet& extensions) {
  for (const auto& extension : extensions) {
    if (extension->name() == name) {
      return true;
    }
  }

  return false;
}

// Each test may have a different desired stage. Store them here so the test
// harness properly instantiates them.
MV2ExperimentStage GetExperimentStageForTest(std::string_view test_name) {
  static constexpr struct {
    const char* test_name;
    MV2ExperimentStage stage;
  } test_stages[] = {
      {"PRE_PRE_PRE_AcknowledgementResetBetweenExperiments",
       MV2ExperimentStage::kDisableWithReEnable},
      {"PRE_PRE_AcknowledgementResetBetweenExperiments",
       MV2ExperimentStage::kDisableWithReEnable},
      {"PRE_AcknowledgementResetBetweenExperiments",
       MV2ExperimentStage::kDisableWithReEnable},
      {"AcknowledgementResetBetweenExperiments",
       MV2ExperimentStage::kUnsupported},
  };

  for (const auto& test_stage : test_stages) {
    if (test_stage.test_name == test_name) {
      return test_stage.stage;
    }
  }

  NOTREACHED()
      << "Unknown test name '" << test_name << "'. "
      << "You need to add a new test stage entry into this collection.";
}

}  // namespace

class Mv2DisabledDialogControllerInteractiveUITest
    : public InteractiveBrowserTest {
 public:
  Mv2DisabledDialogControllerInteractiveUITest() = default;
  ~Mv2DisabledDialogControllerInteractiveUITest() override = default;

  void SetUp() override {
    // Each test may need a different value for the experiment stages, since
    // many need some kind of pre-experiment set up, then test the behavior on
    // subsequent startups. Initialize each test according to its preferred
    // stage.
    MV2ExperimentStage experiment_stage = GetExperimentStageForTest(
        testing::UnitTest::GetInstance()->current_test_info()->name());
    SetupFeatures(experiment_stage);

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Disable overviews in ChromeOS, so they don't appear if you had a windows
    // opened previously (which affects the dialog visibility).
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(
        ash::prefs::kRestoreAppsAndPagesPrefName,
        static_cast<int>(ash::full_restore::RestoreOption::kDoNotRestore));
#endif
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  scoped_refptr<const Extension> AddMV2Extension(
      std::string_view name,
      mojom::ManifestLocation location = mojom::ManifestLocation::kInternal) {
    static constexpr char kManifest[] =
        R"({
             "name": "%s",
             "manifest_version": 2,
             "version": "0.1"
           })";
    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(kManifest, name.data()));

    ChromeTestExtensionLoader loader(browser()->profile());
    loader.set_location(location);
    // TODO(crbug.com/40269105): The packing step creates a _metadata folder
    // which causes an install warning when loading.
    loader.set_ignore_manifest_warnings(true);
    scoped_refptr<const Extension> extension =
        loader.LoadExtension(test_dir.Pack());
    return extension;
  }

  scoped_refptr<const Extension> AddMV2ExtensionWithIcon(
      std::string_view name,
      std::string icon_file) {
    static constexpr char kManifest[] =
        R"({
             "name": "%s",
             "manifest_version": 2,
             "version": "0.1",
             "icons": {
                "16": "icon16.png"
             }
           })";
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath icon_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII("extensions")
            .AppendASCII(icon_file);
    std::string icon_file_content;
    EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_file_content));

    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(kManifest, name.data()));
    test_dir.WriteFile(FILE_PATH_LITERAL("icon16.png"), icon_file_content);

    ChromeTestExtensionLoader loader(browser()->profile());
    // TODO(crbug.com/40269105): The packing step creates a _metadata folder
    // which causes an install warning when loading.
    loader.set_ignore_manifest_warnings(true);
    scoped_refptr<const Extension> extension =
        loader.LoadExtension(test_dir.Pack());
    return extension;
  }

  MV2ExperimentStage GetActiveExperimentStage() {
    return experiment_manager()->GetCurrentExperimentStage();
  }

  ExtensionRegistry* extension_registry() {
    return ExtensionRegistry::Get(browser()->profile());
  }

  ManifestV2ExperimentManager* experiment_manager() {
    return ManifestV2ExperimentManager::Get(browser()->profile());
  }

 protected:
  void SetupFeatures(MV2ExperimentStage experiment_stage) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (experiment_stage) {
      case MV2ExperimentStage::kWarning:
      case MV2ExperimentStage::kNone:
        NOTREACHED() << "Unhandled stage.";
      case MV2ExperimentStage::kDisableWithReEnable:
        enabled_features.push_back(
            extensions_features::kExtensionManifestV2Disabled);
        disabled_features.push_back(
            extensions_features::kExtensionManifestV2Unsupported);
        break;
      case MV2ExperimentStage::kUnsupported:
        enabled_features.push_back(
            extensions_features::kExtensionManifestV2Unsupported);
        disabled_features.push_back(
            extensions_features::kExtensionManifestV2Disabled);
        break;
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Disable install verification.
  ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

class Mv2DisabledDialogControllerInteractiveUITestWithParam
    : public Mv2DisabledDialogControllerInteractiveUITest,
      public testing::WithParamInterface<MV2ExperimentStage> {
 public:
  void SetUp() override {
    MV2ExperimentStage experiment_stage = GetParam();
    SetupFeatures(experiment_stage);

    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    Mv2DisabledDialogControllerInteractiveUITestWithParam,
    ::testing::Values(MV2ExperimentStage::kDisableWithReEnable,
                      MV2ExperimentStage::kUnsupported),
    [](const testing::TestParamInfo<MV2ExperimentStage>& info) {
      switch (info.param) {
        case MV2ExperimentStage::kNone:
        case MV2ExperimentStage::kWarning:
          NOTREACHED();
        case MV2ExperimentStage::kDisableWithReEnable:
          return "DisableExperiment";
        case MV2ExperimentStage::kUnsupported:
          return "UnsupportedExperiment";
      }
    });

// Tests that extensions in disable dialog are uninstalled when the remove
// button is selected.
// Stage 1: Install an MV2 extension.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_PRE_OnRemoveSelected) {
  scoped_refptr<const Extension> extension = AddMV2Extension("MV2 Extension");

  // Extension is not affected by the MV2 deprecation, yet. Thus, dialog is not
  // visible.
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
}
// Stage 2: Select the remove option in the disable dialog, which should
// uninstall the extension.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_OnRemoveSelected) {
  RunTestSequence(
      // Extension is disabled due to the MV2 deprecation stage.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      PressButton(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      WaitForHide(kExtensionsMv2DisabledDialogRemoveButtonElementId),

      // Clicking the remove button uninstalls the extension.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension",
                extension_registry()->GenerateInstalledExtensionsSet());
          },
          false));
}
// Stage 3: Dialog is not shown again, since user acknowledged it.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       OnRemoveSelected) {
  RunTestSequence(
      EnsureNotPresent(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}

// Tests that the extensions page is opened when the manage button is selected,
// and the extension is left disabled.
// Stage 1: Install an MV2 extension.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_PRE_OnManageSelected) {
  scoped_refptr<const Extension> extension = AddMV2Extension("MV2 Extension");

  // Extension is not affected by the MV2 deprecation, yet. Thus, dialog is not
  // visible.
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
}
// Stage 2: Select the manage option in the disable dialog, which should open
// the extensions page.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_OnManageSelected) {
  RunTestSequence(
      InstrumentTab(kTabId),
      // Extension is disabled due to the MV2 deprecation stage.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(kExtensionsMv2DisabledDialogManageButtonElementId),
      PressButton(kExtensionsMv2DisabledDialogManageButtonElementId),
      WaitForHide(kExtensionsMv2DisabledDialogManageButtonElementId),
      // Clicking on the manage button should open the extensions page.
      WaitForWebContentsReady(kTabId, GURL("chrome://extensions")),
      // Extension remains disabled.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true));
}
// Stage 3: Dialog is not shown again, since user acknowledged it.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       OnManageSelected) {
  RunTestSequence(
      EnsureNotPresent(kExtensionsMv2DisabledDialogManageButtonElementId));
}

// Tests the dialog is shown again on new sessions if the user didn't take an
// action on the previous one (e.g dialog was closed for other reasons).
// Stage 1: Install an MV2 extension.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_PRE_NoUserAction) {
  scoped_refptr<const Extension> extension = AddMV2Extension("MV2 Extension");

  // Extension is not affected by the MV2 deprecation, yet. Thus, dialog is not
  // visible.
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
}
// Stage 2: Take no action on the dialog.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_NoUserAction) {
  RunTestSequence(
      // Extension is disabled due to the MV2 deprecation stage.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId)
      // End the test without taking a user action on the dialog.
  );
}
// Stage 3: Dialog is shown again, since user didn't take an action the previous
// time it was shown.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       NoUserAction) {
  RunTestSequence(
      // Extension is disabled due to the MV2 deprecation stage.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true),
      // Dialog is shown again.
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}

// Tests that only MV2 disabled extensions that can be uninstalled are included
// in the dialog.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PolicyInstalledExtensions) {
  const ExtensionId internal_extension_id =
      AddMV2Extension("Internal Extension", mojom::ManifestLocation::kInternal)
          ->id();
  const ExtensionId policy_extension_id =
      AddMV2Extension("Policy Extension",
                      mojom::ManifestLocation::kExternalPolicy)
          ->id();
  views::NamedWidgetShownWaiter dialog_show_waiter(
      views::test::AnyWidgetTestPasskey{}, "Mv2DeprecationDisabledDialog");

  RunTestSequence(
      // Extensions are not affected by the MV2 deprecation, yet.
      CheckResult(
          [&]() {
            return extension_registry()->enabled_extensions().Contains(
                internal_extension_id);
          },
          true),
      CheckResult(
          [&]() {
            return extension_registry()->enabled_extensions().Contains(
                policy_extension_id);
          },
          true),
      // Dialog is not visible because there are no extensions disabled due to
      // the MV2 deprecation.
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      EnsureNotPresent(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      // Disable extensions affected by the MV2 deprecation.
      Do([this]() {
        experiment_manager()->DisableAffectedExtensionsForTesting();
      }),
      // Verify both extensions were disabled.
      CheckResult(
          [&]() {
            return extension_registry()->disabled_extensions().Contains(
                internal_extension_id);
          },
          true),
      CheckResult(
          [&]() {
            return extension_registry()->disabled_extensions().Contains(
                policy_extension_id);
          },
          true),
      // Trigger the dialog.
      // Note: We need to manually trigger the dialog because the is no way to
      // preserve policy-installed extensions between tests, and therefore we
      // cannot trigger the dialog in between tests.
      Do([&]() {
        Mv2DisabledDialogController* dialog_controller =
            browser()
                ->browser_window_features()
                ->mv2_disabled_dialog_controller_for_testing();
        dialog_controller->MaybeShowDisabledDialogForTesting();
        views::Widget* dialog_widget = dialog_show_waiter.WaitIfNeededAndGet();
        ASSERT_TRUE(dialog_widget);
      }),
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      // Verify only internal extension is on the dialog.
      // Note: ideally we would verify the extension name in the title dialog.
      // However, there is no way to add an element identifier to the dialog
      // title when it's built using DialogModel::Builder. Therefore, we check
      // dialog paragraph has singular string (dialog only has one extension).
      // This is complemented by next steps which verify which extension was
      // uninstalled.
      CheckViewProperty(
          kExtensionsMv2DisabledDialogParagraphElementId,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_DESCRIPTION,
              /*number=*/1)),
      // Select remove option.
      PressButton(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      WaitForHide(kExtensionsMv2DisabledDialogRemoveButtonElementId),

      // Internal extension is uninstalled but policy extension remains
      // installed.
      CheckResult(
          [&]() {
            return extension_registry()
                ->GenerateInstalledExtensionsSet()
                .Contains(internal_extension_id);
          },
          false),
      CheckResult(
          [&]() {
            return extension_registry()
                ->GenerateInstalledExtensionsSet()
                .Contains(policy_extension_id);
          },
          true));
}

// Tests that icons loaded asynchronously trigger the dialog after load is
// finished.
// Stage 1: Load an MV2 extension with an icon.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_IconsLoaded) {
  scoped_refptr<const Extension> extension_A =
      AddMV2ExtensionWithIcon("Extension A", "icon1.png");
  scoped_refptr<const Extension> extension_B =
      AddMV2ExtensionWithIcon("Extension B", "icon2.png");
  scoped_refptr<const Extension> extension_C = AddMV2Extension("Extension C");

  // Extensions are not affected by the MV2 deprecation, yet. Thus, dialog is
  // not visible.
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_A->id()));
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_B->id()));
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_C->id()));
}
// Stage 2: Dialog should be visible and have icon.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       IconsLoaded) {
  RunTestSequence(
      // Extensions are disabled due to the MV2 deprecation stage.
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "Extension A", extension_registry()->disabled_extensions());
          },
          true),
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "Extension B", extension_registry()->disabled_extensions());
          },
          true),
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "Extension C", extension_registry()->disabled_extensions());
          },
          true),
      // We cannot add an element identifier to the dialog when it's
      // built using DialogModel::Builder. Thus, we check for its
      // existence by checking the visibility of one of its
      // elements.
      WaitForShow(kExtensionsMv2DisabledDialogParagraphElementId),
      // Verify the three extensions are on the dialog.
      // Note: ideally we would verify the extension entries in the
      // dialog. However, we cannot assign a different element id to
      // each entry since the number of entries can change.
      // Therefore, we check whether the dialog paragraph has plural
      // string and that the correct extension info was provided to
      // the dialog.
      CheckViewProperty(
          kExtensionsMv2DisabledDialogParagraphElementId,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(
              IDS_EXTENSIONS_MANIFEST_V2_DEPRECATION_DISABLED_DIALOG_DESCRIPTION,
              /*number=*/3)));
}

// Tests that the correct extension info is passed to the dialog.
// Stage 1: Load two MV2 extensions.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       PRE_CorrectExtensionInfo) {
  scoped_refptr<const Extension> extension_A =
      AddMV2ExtensionWithIcon("Extension A", "icon1.png");
  scoped_refptr<const Extension> extension_B = AddMV2Extension("Extension B");

  // Extensions are not affected by the MV2 deprecation, yet.
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_A->id()));
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_B->id()));
}
// Stage 2: Verify extension info passed to dialog is correct.
IN_PROC_BROWSER_TEST_P(Mv2DisabledDialogControllerInteractiveUITestWithParam,
                       CorrectExtensionInfo) {
  RunTestSequence(
      // Wait for dialog to be visible. Other checks on this test will be done
      // "outside" of it.
      // Note: We cannot add an element identifier to the dialog when
      // it's built using DialogModel::Builder. Thus, we check for its existence
      // by checking the visibility of one of its elements.
      WaitForShow(kExtensionsMv2DisabledDialogParagraphElementId));

  EXPECT_TRUE(HasExtensionByName("Extension A",
                                 extension_registry()->disabled_extensions()));
  EXPECT_TRUE(HasExtensionByName("Extension B",
                                 extension_registry()->disabled_extensions()));

  Mv2DisabledDialogController* dialog_controller =
      browser()
          ->browser_window_features()
          ->mv2_disabled_dialog_controller_for_testing();
  std::vector<Mv2DisabledDialogController::ExtensionInfo> affected_extensions =
      dialog_controller->GetAffectedExtensionsForTesting();

  // Verify extensions are ordered alphabetically, by extension name, and they
  // have an icon.
  EXPECT_EQ(affected_extensions[0].name, "Extension A");
  EXPECT_EQ(affected_extensions[1].name, "Extension B");
  EXPECT_FALSE(affected_extensions[0].icon.IsEmpty());
  EXPECT_FALSE(affected_extensions[0].icon.IsEmpty());
}

// Tests that the disabled dialog is shown again for a new experiment stage even
// if it was acknowledged on a previous experiment stage.
// Stage 1: Install an MV2 extension during 'disabled with re-enabled'
// experiment stage.
IN_PROC_BROWSER_TEST_F(Mv2DisabledDialogControllerInteractiveUITest,
                       PRE_PRE_PRE_AcknowledgementResetBetweenExperiments) {
  EXPECT_EQ(GetActiveExperimentStage(),
            MV2ExperimentStage::kDisableWithReEnable);

  scoped_refptr<const Extension> extension = AddMV2Extension("MV2 Extension");

  // Extension is not affected by the MV2 deprecation, yet. Thus, dialog is not
  // visible.
  RunTestSequence(
      CheckResult(
          [&]() {
            return extension_registry()->enabled_extensions().Contains(
                extension->id());
          },
          true),
      EnsureNotPresent(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}
// Stage 2: Dialog is visible and user acknowledges it during 'disabled with
// re-enabled' experiment stage.
IN_PROC_BROWSER_TEST_F(Mv2DisabledDialogControllerInteractiveUITest,
                       PRE_PRE_AcknowledgementResetBetweenExperiments) {
  EXPECT_EQ(GetActiveExperimentStage(),
            MV2ExperimentStage::kDisableWithReEnable);

  RunTestSequence(
      CheckResult(
          [&]() {
            return HasExtensionByName(
                "MV2 Extension", extension_registry()->disabled_extensions());
          },
          true),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId),
      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      WaitForHide(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}
// Stage 3: Dialog is not shown again during 'disabled with re-enabled'
// experiment stage.
IN_PROC_BROWSER_TEST_F(Mv2DisabledDialogControllerInteractiveUITest,
                       PRE_AcknowledgementResetBetweenExperiments) {
  EXPECT_EQ(GetActiveExperimentStage(),
            MV2ExperimentStage::kDisableWithReEnable);

  RunTestSequence(
      EnsureNotPresent(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}
// Stage 4: Dialog is shown again for 'unsupported' experiment stage.
IN_PROC_BROWSER_TEST_F(Mv2DisabledDialogControllerInteractiveUITest,
                       AcknowledgementResetBetweenExperiments) {
  EXPECT_EQ(GetActiveExperimentStage(), MV2ExperimentStage::kUnsupported);

  RunTestSequence(
      WaitForShow(kExtensionsMv2DisabledDialogRemoveButtonElementId));
}

}  // namespace extensions
