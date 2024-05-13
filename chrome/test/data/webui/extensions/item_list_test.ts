// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-item-list. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsItemListElement} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
        safetyCheckText: {panelString: 'This extension contains malware.'},
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
    itemList.isMv2DeprecationWarningDismissed = false;
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

  test('SafetyCheckPanel', function() {
    // The extension review panel should not be visible if
    // safetyCheckShowReviewPanel and safetyHubShowReviewPanel are set to
    // false.
    loadTimeData.overrideValues({'safetyCheckShowReviewPanel': false});
    loadTimeData.overrideValues({'safetyHubShowReviewPanel': false});

    // set up the element again to capture the updated value of
    // safetyCheckShowReviewPanel.
    setupElement();

    flush();
    boundTestVisible('extensions-review-panel', false);
    // The extension review panel should be visible if the feature flag is set
    // to true.
    loadTimeData.overrideValues({'safetyCheckShowReviewPanel': true});

    // set up the element again to capture the updated value of
    // safetyCheckShowReviewPanel.
    setupElement();

    flush();
    boundTestVisible('extensions-review-panel', true);

    // The extension review panel should  be visible if
    // safetyHubShowReviewPanel is set to true.
    loadTimeData.overrideValues({'safetyCheckShowReviewPanel': false});
    loadTimeData.overrideValues({'safetyHubShowReviewPanel': true});
    setupElement();

    flush();
    boundTestVisible('extensions-review-panel', true);
  });

  test('ManifestV2DeprecationPanel_Disabled', async function() {
    // Panel is hidden if panel is disabled.
    loadTimeData.overrideValues({'MV2DeprecationPanelEnabled': false});
    setupElement();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });

  test('ManifestV2DeprecationPanel_Enabled', async function() {
    // Panel is hidden if panel is enabled and has no extensions affected
    // by the MV2 deprecation.
    loadTimeData.overrideValues({'MV2DeprecationPanelEnabled': true});
    setupElement();
    boundTestVisible('extensions-mv2-deprecation-panel', false);

    // Panel is visible if panel is enabled and has at least one extension
    // affected by the MV2 deprecation.
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

    // Panel is visible if panel is enabled and has multiple extensions affected
    // by the MV2 deprecation.
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
                    didAcknowledgeMV2DeprecationWarning: true,
                  }));
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', true);
    // The length remains at 2.
    assertEquals(2, mv2DeprecationPanel.extensions.length);

    // Panel is hidden if warning has been dismissed.
    itemList.set('isMv2DeprecationWarningDismissed', true);
    flush();
    boundTestVisible('extensions-mv2-deprecation-panel', false);
  });
});
