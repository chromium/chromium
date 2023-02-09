// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OSSettingsAccessibilityV3Test fixture.
GEN_INCLUDE([
  'os_settings_accessibility_v3_test.js',
]);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/ash/crostini/crostini_pref_names.h"');
GEN('#include "chrome/browser/ash/crostini/fake_crostini_features.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/prefs/pref_service.h"');
GEN('#include "content/public/test/browser_test.h"');

// TODO(crbug.com/1002627): This block prevents generation of a
// link-in-text-block browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// OSSettingsAccessibilityV3Test.axeOptions
const axeOptionsExcludeLinkInTextBlock =
    Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions, {
      'rules':
          Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions.rules, {
            'link-in-text-block': {enabled: false},
          }),
    });

// TODO(crbug.com/1180696): This block prevents generation of a
// document-title browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// OSSettingsAccessibilityV3Test.axeOptions
const axeOptionsDocumentTitle =
    Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions, {
      'rules':
          Object.assign({}, OSSettingsAccessibilityV3Test.axeOptions.rules, {
            'document-title': {enabled: false},
          }),
    });

const multideviceFeatureViolationFilter =
    Object.assign({}, OSSettingsAccessibilityV3Test.violationsFilter, {
      // Excuse link without an underline.
      // TODO(https://crbug.com/894602): Remove this exception when settled
      // with UX.
      'link-in-text-block': function(nodeResult) {
        return nodeResult.element.parentElement.id === 'multideviceSubLabel';
      },
    });

const crostiniFeatureList = {
  enabled: ['features::kCrostini'],
};

function crostiniTestGenPreamble() {
  GEN('browser()->profile()->GetPrefs()->SetBoolean(');
  GEN('    crostini::prefs::kCrostiniEnabled, true);');
  GEN('crostini::FakeCrostiniFeatures fake_crostini_features;');
  GEN('fake_crostini_features.SetAll(true);');
}

const crostiniConfig = {
  featureList: crostiniFeatureList,
  options: axeOptionsDocumentTitle,
  testGenPreamble: crostiniTestGenPreamble,
};

const crostiniSubpageOptionsRules = Object.assign(
    {}, axeOptionsDocumentTitle.rules, axeOptionsExcludeLinkInTextBlock.rules);

const crostiniSubpageConfig = {
  featureList: crostiniFeatureList,
  options: Object.assign(
      {}, OSSettingsAccessibilityV3Test.axeOptions,
      {'rules': crostiniSubpageOptionsRules}),
  testGenPreamble: crostiniTestGenPreamble,
};

[[
  'Basic',
  'basic_a11y_v3_test.js',
  {options: axeOptionsExcludeLinkInTextBlock},
],
 [
   'GoogleAssistant',
   'google_assistant_a11y_v3_test.js',
   {options: axeOptionsDocumentTitle},
 ],
 [
   'ManageAccessibility',
   'manage_accessibility_a11y_v3_test.js',
   {options: axeOptionsDocumentTitle},
 ],
 [
   'Multidevice',
   'multidevice_a11y_v3_test.js',
   {options: axeOptionsDocumentTitle},
 ],
 [
   'CrostiniDetails',
   'crostini_settings_details_a11y_v3_test.js',
   crostiniConfig,
 ],
 [
   'CrostiniExportImport',
   'crostini_settings_export_import_a11y_v3_test.js',
   crostiniConfig,
 ],
 [
   'CrostiniSharedPaths',
   'crostini_settings_shared_paths_a11y_v3_test.js',
   crostiniConfig,
 ],
 [
   'CrostiniSharedUsbDevices',
   'crostini_settings_shared_usb_devices_a11y_v3_test.js',
   crostiniConfig,
 ],
 [
   'CrostiniSubpage',
   'crostini_settings_subpage_a11y_v3_test.js',
   crostiniSubpageConfig,
 ],
 [
   'MultideviceFeatures',
   'multidevice_features_a11y_v3_test.js',
   {
     filter: multideviceFeatureViolationFilter,
     options: axeOptionsDocumentTitle,
   },
 ],
 [
   'Tts',
   'tts_subpage_a11y_v3_test.js',
   {
     switches: ['enable-experimental-a11y-features'],
     options: axeOptionsDocumentTitle,
   },
 ],
].forEach(test => defineTest(...test));

function defineTest(testName, module, config) {
  const className = `OSSettingsA11y${testName}V3`;
  this[className] = class extends OSSettingsAccessibilityV3Test {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/a11y/${
          module}`;
    }

    /** @override */
    get commandLineSwitches() {
      if (config && config.switches) {
        return config.switches;
      }
      return [];
    }

    /** @override */
    get featureList() {
      if (config && config.featureList) {
        return config.featureList;
      }
      return undefined;
    }

    /** @override */
    testGenPreamble() {
      if (config && config.testGenPreamble) {
        return config.testGenPreamble();
      }
    }
  };

  const filter = config && config.filter ?
      config.filter :
      OSSettingsAccessibilityV3Test.violationFilter;
  const options = config && config.options ?
      config.options :
      OSSettingsAccessibilityV3Test.axeOptions;
  AccessibilityTest.define(className, {
    /** @override */
    name: testName,
    /** @override */
    axeOptions: options,
    /** @override */
    tests: {'All': function() {}},
    /** @override */
    violationFilter: filter,
  });
}
