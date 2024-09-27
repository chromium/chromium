// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-item-list. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsItemListElement} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo, testVisible} from './test_util.js';

suite('ExtensionItemListTest', function() {
  let itemList: ExtensionsItemListElement;
  let boundTestVisible: (selector: string, visible: boolean, text?: string) =>
      void;

  // Initialize an extension item before each test.
  setup(function() {
    setupElement();
  });

  function setupElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    itemList = document.createElement('extensions-item-list');
    boundTestVisible = testVisible.bind(null, itemList);

    const createExt = createExtensionInfo;
    const extensionItems = [
      createExt({
        name: 'Alpha',
        id: 'a'.repeat(32),
      }),
      createExt({name: 'Bravo', id: 'b'.repeat(32)}),
      createExt({name: 'Charlie', id: 'c'.repeat(29) + 'wxy'}),
    ];
    const appItems = [
      createExt({name: 'QQ', id: 'q'.repeat(32)}),
    ];
    itemList.extensions = extensionItems;
    itemList.apps = appItems;
    itemList.filter = '';
    itemList.isMv2DeprecationNoticeDismissed = false;
    document.body.appendChild(itemList);
  }

  test('Filtering', function() {
    function itemLengthEquals(num: number) {
      flush();
      assertEquals(
          itemList.shadowRoot!.querySelectorAll('extensions-item').length, num);
    }

    // We should initially show all the items.
    itemLengthEquals(4);

    // All extension items have an 'a'.
    itemList.filter = 'a';
    itemLengthEquals(3);
    // Filtering is case-insensitive, so all extension items should be shown.
    itemList.filter = 'A';
    itemLengthEquals(3);
    // Only 'Bravo' has a 'b'.
    itemList.filter = 'b';
    itemLengthEquals(1);
    assertEquals(
        'Bravo',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test inner substring (rather than prefix).
    itemList.filter = 'lph';
    itemLengthEquals(1);
    assertEquals(
        'Alpha',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test trailing/leading spaces.
    itemList.filter = '   Alpha  ';
    itemLengthEquals(1);
    assertEquals(
        'Alpha',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test string with no matching items.
    itemList.filter = 'z';
    itemLengthEquals(0);
    // A filter of '' should reset to show all items.
    itemList.filter = '';
    itemLengthEquals(4);
    // A filter of 'q' should should show just the apps item.
    itemList.filter = 'q';
    itemLengthEquals(1);
    // A filter of 'xy' should show just the 'Charlie' item since its id
    // matches.
    itemList.filter = 'xy';
    itemLengthEquals(1);
    assertEquals(
        'Charlie',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
  });

  test('NoItems', function() {
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', false);

    itemList.extensions = [];
    itemList.apps = [];
    flush();
    boundTestVisible('#no-items', true);
    boundTestVisible('#no-search-results', false);
  });

  test('NoSearchResults', function() {
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', false);

    itemList.filter = 'non-existent name';
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', true);
  });

  // Tests that the extensions section and the chrome apps section, along with
  // their headers, are only visible when extensions or chrome apps are
  // existent, respectively. Otherwise, no items section is shown.
  test('SectionsVisibility', function() {
    flush();

    // Extensions and chrome apps were added during setup.
    boundTestVisible('#extensions-section', true);
    boundTestVisible('#extensions-section h2.section-header', true);
    boundTestVisible('#chrome-apps-section', true);
    boundTestVisible('#chrome-apps-section h2.section-header', true);
    boundTestVisible('#no-items', false);

    itemList.apps = [];
    flush();

    // Verify chrome apps section is not visible when there are no chrome apps.
    boundTestVisible('#extensions-section', true);
    boundTestVisible('#extensions-section h2.section-header', true);
    boundTestVisible('#chrome-apps-section', false);
    boundTestVisible('#chrome-apps-section h2.section-header', false);
    boundTestVisible('#no-items', false);

    itemList.extensions = [];
    flush();

    // Verify extensions section is not visible when there are no extensions.
    // Since there are no extensions or chrome apps, no items section is
    // displayed.
    boundTestVisible('#extensions-section', false);
    boundTestVisible('#extensions-section h2.section-header', false);
    boundTestVisible('#chrome-apps-section', false);
    boundTestVisible('#chrome-apps-section h2.section-header', false);
    boundTestVisible('#no-items', true);
  });

  test('LoadTimeData', function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('browserManagedByOrg');
  });

  test('SafetyCheckPanel_Disabled', function() {
    // Panel is hidden if safetyCheckShowReviewPanel and
    // safetyHubShowReviewPanel are disabled.
    loadTimeData.overrideValues(
        {safetyCheckShowReviewPanel: false, safetyHubShowReviewPanel: false});
    setupElement();
    flush();
    boundTestVisible('extensions-review-panel', false);
  });

  test('SafetyCheckPanel_EnabledSafetyCheck', function() {
    // Panel is hidden if safetyCheckShowReviewPanel is enabled, there are no
    // unsafe extensions and panel wasn't previously shown.
    loadTimeData.overrideValues(
        {safetyCheckShowReviewPanel: true, safetyHubShowReviewPanel: true});
    setupElement();
    flush();
    boundTestVisible('extensions-review-panel', false);

    // Panel is visible if safetyCheckShowReviewPanel is enabled, and there are
    // unsafe extensions.
    itemList.push(
        'extensions', createExtensionInfo({
          name: 'Unsafe extension',
          id: 'd'.repeat(32),
          safetyCheckText: {panelString: 'This extension contains malware.'},
        }));
    flush();
    boundTestVisible('extensions-review-panel', true);
    const reviewPanel =
        itemList.shadowRoot!.querySelector('extensions-review-panel');
    assertTrue(!!reviewPanel);
    assertEquals(1, reviewPanel.extensions.length);
  });

  test('SafetyCheckPanel_EnabledSafetyHub', function() {
    // Panel is hidden if safetyHubShowReviewPanel is enabled, there are no
    // unsafe extensions and panel wasn't previously shown.
    loadTimeData.overrideValues(
        {safetyCheckShowReviewPanel: false, safetyHubShowReviewPanel: true});
    setupElement();
    flush();
    boundTestVisible('extensions-review-panel', false);

    // Panel is visible if safetyHubShowReviewPanel is enabled, and there are
    // unsafe extensions.
    itemList.push(
        'extensions', createExtensionInfo({
          name: 'Unsafe extension',
          id: 'd'.repeat(32),
          safetyCheckText: {panelString: 'This extension contains malware.'},
        }));
    flush();
    boundTestVisible('extensions-review-panel', true);
    const reviewPanel =
        itemList.shadowRoot!.querySelector('extensions-review-panel');
    assertTrue(!!reviewPanel);
    assertEquals(1, reviewPanel.extensions.length);
  });

  test('ManifestV2DeprecationPanel_None', async function() {
    // Panel is hidden for experiment on stage 0 (none).
    loadTimeData.overrideValues({MV2ExperimentStage: 0});
    setupElement();
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });

  test('ManifestV2DeprecationPanel_Warning', async function() {
    // Panel is hidden for experiment on stage 1 (warning) and has no extensions
    // affected by the MV2 deprecation.
    loadTimeData.overrideValues({MV2ExperimentStage: 1});
    setupElement();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is visible for experiment on stage 1 (warning) and has at least one
    // extension affected by the MV2 deprecation.
    itemList.push('extensions', createExtensionInfo({
                    name: 'Extension D',
                    id: 'd'.repeat(32),
                    isAffectedByMV2Deprecation: true,
                  }));
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    const mv2DeprecationPanel =
        itemList.shadowRoot!.querySelector('extensions-mv2-deprecation-panel');
    assertTrue(!!mv2DeprecationPanel);
    assertEquals(1, mv2DeprecationPanel.extensions.length);

    // Panel is visible for experiment on stage 1 (warning) and has multiple
    // extensions affected by the MV2 deprecation.
    itemList.push('extensions', createExtensionInfo({
                    name: 'Extension E',
                    id: 'e'.repeat(32),
                    isAffectedByMV2Deprecation: true,
                  }));
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    assertEquals(2, mv2DeprecationPanel.extensions.length);

    // Extensions that are affected by the MV2 deprecation, but have already
    // been acknowledged, are not included in the list.
    itemList.push('extensions', createExtensionInfo({
                    name: 'Extension F',
                    id: 'f'.repeat(32),
                    isAffectedByMV2Deprecation: true,
                    didAcknowledgeMV2DeprecationNotice: true,
                  }));
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    // The length remains at 2.
    assertEquals(2, mv2DeprecationPanel.extensions.length);

    // Panel is hidden if notice has been dismissed.
    itemList.set('isMv2DeprecationNoticeDismissed', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });

  test('ManifestV2DeprecationPanel_DisableWithReEnable', async function() {
    // Panel is hidden for experiment on stage 2 (disable with re-enable) when
    // it has no extensions affected by the MV2 deprecation.
    loadTimeData.overrideValues({MV2ExperimentStage: 2});
    setupElement();
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is hidden for experiment on stage 2 (disable with re-enable)
    // when extension is affected by the MV2 deprecation but it's not disabled
    // due to unsupported manifest version.
    // Note: This can happen when the user chose to re-enable a MV2 disabled
    // extension.
    itemList.set('extensions.0.isAffectedByMV2Deprecation', true);
    itemList.set(
        'extensions.0.disableReasons.unsupportedManifestVersion', false);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is visible for experiment on stage 2 (disable with re-enable)
    // when extension is affected by the MV2 deprecation and extension is
    // disabled due to unsupported manifest version.
    itemList.set(
        'extensions.0.disableReasons.unsupportedManifestVersion', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    const mv2DeprecationPanel =
        itemList.shadowRoot!.querySelector('extensions-mv2-deprecation-panel');
    assertTrue(!!mv2DeprecationPanel);
    assertEquals(1, mv2DeprecationPanel.extensions.length);

    // Panel is visible for experiment on stage 2 (disable with re-enable) and
    // has multiple extensions affected by the MV2 deprecation that are disabled
    // due to unsupported manifest version.
    itemList.set('extensions.1.isAffectedByMV2Deprecation', true);
    itemList.set(
        'extensions.1.disableReasons.unsupportedManifestVersion', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    assertEquals(2, mv2DeprecationPanel.extensions.length);

    // Extensions that are affected by the MV2 deprecation, but have already
    // been acknowledged, are not included in the list.
    itemList.set('extensions.1.didAcknowledgeMV2DeprecationNotice', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    // Panel has only one extension.
    assertEquals(1, mv2DeprecationPanel.extensions.length);

    // Panel is hidden if notice has been dismissed.
    itemList.set('isMv2DeprecationNoticeDismissed', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });

  test('ManifestV2DeprecationPanel_Unsupported', async function() {
    // Panel is hidden for experiment on stage 3 (unsupported) when
    // it has no extensions affected by the MV2 deprecation.
    loadTimeData.overrideValues({MV2ExperimentStage: 3});
    setupElement();
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is hidden for experiment on stage 3 (unsupported) when extension is
    // affected by the MV2 deprecation but it's not disabled due to unsupported
    // manifest version.
    itemList.set('extensions.0.isAffectedByMV2Deprecation', true);
    itemList.set(
        'extensions.0.disableReasons.unsupportedManifestVersion', false);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is visible for experiment on stage 3 (unsupported) when extension
    // is affected by the MV2 deprecation and extension is disabled due to
    // unsupported manifest version.
    itemList.set(
        'extensions.0.disableReasons.unsupportedManifestVersion', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    const mv2DeprecationPanel =
        itemList.shadowRoot!.querySelector('extensions-mv2-deprecation-panel');
    assertTrue(!!mv2DeprecationPanel);
    assertEquals(1, mv2DeprecationPanel.extensions.length);

    // Panel is visible for experiment on stage 3 (unsupported) and has multiple
    // extensions affected by the MV2 deprecation that are disabled due to
    // unsupported manifest version.
    itemList.set('extensions.1.isAffectedByMV2Deprecation', true);
    itemList.set(
        'extensions.1.disableReasons.unsupportedManifestVersion', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    assertEquals(2, mv2DeprecationPanel.extensions.length);

    // Panel is hidden if notice has been dismissed for this stage.
    itemList.set('isMv2DeprecationNoticeDismissed', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });

  test('ManifestV2DeprecationPanel_TitleVisibility', async () => {
    // Enable feature for both panels (mv2 panel is enabled for stage 1). Their
    // visibility will be determined whether they have extensions to show.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: 1, safetyHubShowReviewPanel: true});
    setupElement();
    flush();
    await microtasksFinished();

    // Both panels should be hidden since they don't have extensions to show.
    boundTestVisible('extensions-mv2-deprecation-panel', false);
    boundTestVisible('extensions-review-panel', false);

    // Show the MV2 deprecation panel by adding an extension affected by the
    // mv2 deprecation.
    itemList.extensions = [
      ...itemList.extensions,
      createExtensionInfo({
        name: 'MV2 extension',
        id: 'd'.repeat(32),
        isAffectedByMV2Deprecation: true,
      }),
    ];
    flush();
    await microtasksFinished();
    boundTestVisible('extensions-mv2-deprecation-panel', true);

    // MV2 deprecation panel title is hidden when the review panel is hidden.
    const mv2DeprecationPanel = itemList.shadowRoot!.querySelector<HTMLElement>(
        'extensions-mv2-deprecation-panel');
    assertTrue(!!mv2DeprecationPanel);
    testVisible(mv2DeprecationPanel, '.panel-title', false);

    // Show the review panel by adding an extension with safety check text.
    itemList.extensions = [
      ...itemList.extensions,
      createExtensionInfo({
        name: 'Unsafe extension',
        id: 'e'.repeat(32),
        safetyCheckText: {panelString: 'This extension contains malware.'},
      }),
    ];
    flush();
    await microtasksFinished();
    boundTestVisible('extensions-review-panel', true);

    // MV2 deprecation panel title is visible when the review panel is visible.
    testVisible(mv2DeprecationPanel, '.panel-title', true);
  });
});
