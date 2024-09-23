// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://system/shared/key_value_pair_viewer/key_value_pair_viewer.js';
import 'chrome://system/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {COLLAPSE_THRESHOLD} from 'chrome://system/shared/key_value_pair_viewer/key_value_pair_entry.js';
import type {KeyValuePairEntry, KeyValuePairEntryElement} from 'chrome://system/shared/key_value_pair_viewer/key_value_pair_entry.js';
import type {KeyValuePairViewerElement} from 'chrome://system/shared/key_value_pair_viewer/key_value_pair_viewer.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

export const ENTRIES: KeyValuePairEntry[] = [
  {key: 'CHROME VERSION', value: '1.2.3.4'},
  {key: 'OS VERSION', value: 'Linux'},
  {key: 'Related Website Sets', value: 'Disabled'},
  {
    key: 'device_event_log',
    value: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {
    key: 'extensions',
    value: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {key: 'graphite_enabled', value: 'false'},
  {
    key: 'mem_usage',
    value: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {
    key: 'mem_usage_with_title',
    value: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {key: 'network_event_log', value: ''},
];

suite('KeyValuePairViewerTest', function() {
  let element: KeyValuePairViewerElement;
  let collapsibleEntries: KeyValuePairEntryElement[];

  function getCollapsibleEntries(): KeyValuePairEntryElement[] {
    const entries = Array.from(
        element.shadowRoot!.querySelectorAll('key-value-pair-entry'));
    return entries.filter((entry: KeyValuePairEntryElement) => {
      return entry.shadowRoot!.querySelector('button:not([hidden])') !== null;
    });
  }

  setup(async function() {
    document.body.innerHTML =
        getTrustedHtml(`<key-value-pair-viewer></key-value-pair-viewer>`);
    element = document.body.querySelector('key-value-pair-viewer')!;
    element.entries = ENTRIES;
    element.loading = false;
    await microtasksFinished();
    collapsibleEntries = getCollapsibleEntries();
  });

  test('SpinnerVisibilityTest', async function() {
    element.loading = true;
    await microtasksFinished();
    assertTrue(isVisible(element.$.spinner));

    element.loading = false;
    await microtasksFinished();
    assertFalse(isVisible(element.$.spinner));
  });

  test('Layout', function() {
    // Table title
    assertTrue(isVisible(element.$.tableTitle));
    assertEquals(
        loadTimeData.getString('tableTitle'), element.$.tableTitle.textContent);

    // Expand / Collapse All button
    assertTrue(isVisible(element.$.expandAll));
    assertTrue(isVisible(element.$.collapseAll));

    // Loading status
    assertFalse(isVisible(element.$.status));

    // Table
    assertTrue(isVisible(element.$.table));
  });

  function expandFirstAndLastCollapsibleLogEntries() {
    assertTrue(collapsibleEntries.length > 0);
    collapsibleEntries[0]!.collapsed = false;
    collapsibleEntries[collapsibleEntries.length - 1]!.collapsed = false;
    return microtasksFinished();
  }

  test('ExpandAll button expands all collapsible cells', async function() {
    await expandFirstAndLastCollapsibleLogEntries();

    element.$.expandAll.click();
    await microtasksFinished();

    for (const entry of collapsibleEntries) {
      assertFalse(entry.collapsed);
    }
  });

  test('CollapseAll button collapses all collapsible cells', async function() {
    await expandFirstAndLastCollapsibleLogEntries();

    element.$.collapseAll.click();
    await microtasksFinished();

    for (const entry of collapsibleEntries) {
      assertTrue(entry.collapsed);
    }
  });
});
