// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://discards/memory_tab.js';

import {durationToString, maybeMakePlural} from 'chrome://discards/discards.js';
import type {TabDiscardsInfo} from 'chrome://discards/discards.mojom-webui.js';
import {CanFreeze} from 'chrome://discards/discards.mojom-webui.js';
import {getSortFunctionForKey} from 'chrome://discards/discards_tab.js';
import {LifecycleUnitDiscardReason, LifecycleUnitLoadingState} from 'chrome://discards/lifecycle_unit_state.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('discards', function() {
  test('GetSortFunctionForKey', function() {
    const dummy1: TabDiscardsInfo = {
      title: 'title 1',
      tabUrl: 'http://urlone.com',
      visibility: 0,  // hidden
      state: 0,       // active
      isAutoDiscardable: false,
      discardCount: 0,
      utilityRank: 0,
      lastActiveSeconds: 0,
      // Dummy default values.
      loadingState: LifecycleUnitLoadingState.UNLOADED,
      canDiscard: false,
      hasFocus: false,
      cannotDiscardReasons: [],
      canFreeze: CanFreeze.YES,
      cannotFreezeReasons: [],
      discardReason: LifecycleUnitDiscardReason.EXTERNAL,
      id: 0,
      siteEngagementScore: 0,
      stateChangeTime: new Date(0),
    };
    const dummy2: TabDiscardsInfo = {
      title: 'title 2',
      tabUrl: 'http://urltwo.com',
      visibility: 1,  // occluded
      state: 3,       // frozen
      isAutoDiscardable: true,
      discardCount: 1,
      utilityRank: 1,
      lastActiveSeconds: 1,
      // Dummy default values.
      loadingState: LifecycleUnitLoadingState.UNLOADED,
      canDiscard: false,
      hasFocus: false,
      cannotDiscardReasons: [],
      canFreeze: CanFreeze.YES,
      cannotFreezeReasons: [],
      discardReason: LifecycleUnitDiscardReason.EXTERNAL,
      id: 0,
      siteEngagementScore: 0,
      stateChangeTime: new Date(0),
    };

    ['title', 'tabUrl', 'visibility', 'state', 'isAutoDiscardable',
     'discardCount', 'utilityRank', 'lastActiveSeconds']
        .forEach((sortKey) => {
          assertTrue(getSortFunctionForKey(sortKey)(dummy1, dummy2) < 0);
          assertTrue(getSortFunctionForKey(sortKey)(dummy2, dummy1) > 0);
          assertTrue(getSortFunctionForKey(sortKey)(dummy1, dummy1) === 0);
          assertTrue(getSortFunctionForKey(sortKey)(dummy2, dummy2) === 0);
        });
  });

  test('DurationToString', function() {
    // Test cases have the form [ 'expected output', input_in_seconds ].
    ([
      ['just now', 0],
      ['just now', 10],
      ['just now', 59],
      ['1 minute ago', 60],
      ['10 minutes ago', 10 * 60 + 30],
      ['59 minutes ago', 59 * 60 + 59],
      ['1 hour ago', 60 * 60],
      ['1 hour and 1 minute ago', 61 * 60],
      ['1 hour and 10 minutes ago', 70 * 60 + 30],
      ['1 day ago', 24 * 60 * 60],
      ['2 days ago', 2.5 * 24 * 60 * 60],
      ['6 days ago', 6.9 * 24 * 60 * 60],
      ['over 1 week ago', 7 * 24 * 60 * 60],
      ['over 2 weeks ago', 2.5 * 7 * 24 * 60 * 60],
      ['over 4 weeks ago', 30 * 24 * 60 * 60],
      ['over 1 month ago', 30.5 * 24 * 60 * 60],
      ['over 2 months ago', 2.5 * 30.5 * 24 * 60 * 60],
      ['over 11 months ago', 364 * 24 * 60 * 60],
      ['over 1 year ago', 365 * 24 * 60 * 60],
      ['over 2 years ago', 2.3 * 365 * 24 * 60 * 60],
    ] as Array<[string, number]>)
        .forEach((data) => {
          assertEquals(data[0], durationToString(data[1]));
        });
  });

  test('MaybeMakePlural', function() {
    assertEquals('hours', maybeMakePlural('hour', 0));
    assertEquals('hour', maybeMakePlural('hour', 1));
    assertEquals('hours', maybeMakePlural('hour', 2));
    assertEquals('hours', maybeMakePlural('hour', 10));
  });

  test('MemoryTabBlobStorageSubMetrics', async function() {
    const mb = 1024 * 1024;
    let currentNumericEntries: {[key: string]: bigint} = {
      'effective_size': BigInt(15 * mb),
      'disk_usage': BigInt(2 * mb),
      'blob_count': 42n,
      'creator_identity:https://small.example.com/': BigInt(4 * mb),
      'creator_identity:https://large.example.com/': BigInt(10 * mb),
      'creator_identity:Unknown Creator': BigInt(1 * mb),
    };

    const memoryTab = document.createElement('memory-tab');
    (memoryTab as unknown as {
      discardsDetailsProvider_: {getProcessMemoryDumps: () => Promise<unknown>},
    }).discardsDetailsProvider_ = {
      getProcessMemoryDumps: () => Promise.resolve({
        success: true,
        processDescriptions: [{pid: {pid: 100}, description: 'Browser'}],
        processDumps: [{
          pid: {pid: 100},
          processType: 0,
          osDump: {
            residentSetKb: 200 * 1024,
            peakResidentSetKb: 200 * 1024,
            isPeakRssResettable: false,
            privateFootprintKb: 100 * 1024,
            sharedFootprintKb: 10 * 1024,
          },
          chromeAllocatorDumps: {
            'site_storage/blob_storage': {
              numericEntries: currentNumericEntries,
              entries: {},
            },
          },
        }],
      }),
    };

    document.body.appendChild(memoryTab);
    await microtasksFinished();

    const shadowRoot = memoryTab.shadowRoot;
    assertTrue(Boolean(shadowRoot));

    // Expand the Browser process row.
    const processRow =
        shadowRoot.querySelector<HTMLElement>('.process-row[data-pid="100"]');
    assertTrue(Boolean(processRow));
    processRow!.click();
    await microtasksFinished();

    // Find the Blob Storage metric row in "Breakdown per component".
    const blobSelector = 'tr.expandable-metric-row' +
        '[data-metric-id="site_storage/blob_storage:effective_size"]';
    const blobRow = shadowRoot.querySelector<HTMLElement>(blobSelector);
    assertTrue(Boolean(blobRow));
    assertEquals(
        'Blob Storage',
        blobRow!.querySelector('.sub-td-label')!.textContent.replace('▶', '')
            .trim());
    assertEquals(
        '15.0 MB', blobRow!.querySelector('.sub-td-val')!.textContent.trim());

    // Sub-metrics are collapsed initially.
    assertEquals(0, shadowRoot.querySelectorAll('tr.sub-metric-row').length);

    // Click Blob Storage row to expand sub-metrics.
    blobRow!.click();
    await microtasksFinished();

    const getSubMetricPairs = (): Array<[string, string]> => {
      return Array.from(shadowRoot.querySelectorAll('tr.sub-metric-row'))
          .map(
              row =>
                  [row.querySelector('.sub-td-label')!.textContent.trim(),
                   row.querySelector('.sub-td-val')!.textContent.trim(),
      ]);
    };

    assertDeepEquals(
        [
          ['Disk Usage', '2.0 MB'],
          ['Blob Count', '42'],
          ['Creator: https://large.example.com/', '10.0 MB'],
          ['Creator: https://small.example.com/', '4.0 MB'],
          ['Creator: Unknown Creator', '1.0 MB'],
        ],
        getSubMetricPairs());

    // Simulate a second update where https://large.example.com/ freed all its
    // blobs (entry omitted from numericEntries by BlobStorageContext).
    currentNumericEntries = {
      'effective_size': BigInt(5 * mb),
      'disk_usage': BigInt(2 * mb),
      'blob_count': 10n,
      'creator_identity:https://small.example.com/': BigInt(4 * mb),
      'creator_identity:Unknown Creator': BigInt(1 * mb),
    };
    await (memoryTab as unknown as {
      fetchData_: () => Promise<void>,
    }).fetchData_();
    await microtasksFinished();

    // https://large.example.com/ remains visible from history with 0.0 MB and
    // sorts after active creators.
    assertDeepEquals(
        [
          ['Disk Usage', '2.0 MB'],
          ['Blob Count', '10'],
          ['Creator: https://small.example.com/', '4.0 MB'],
          ['Creator: Unknown Creator', '1.0 MB'],
          ['Creator: https://large.example.com/', '0.0 MB'],
        ],
        getSubMetricPairs());

    // Click Blob Storage row again to collapse sub-metrics.
    const updatedBlobRow = shadowRoot.querySelector<HTMLElement>(blobSelector);
    assertTrue(Boolean(updatedBlobRow));
    assertFalse(
        updatedBlobRow!.querySelector('.toggle-arrow') === null ||
        !updatedBlobRow!.querySelector('.toggle-arrow')!.classList.contains(
            'expanded'));
    updatedBlobRow!.click();
    await microtasksFinished();
    assertEquals(0, shadowRoot.querySelectorAll('tr.sub-metric-row').length);

    memoryTab.remove();
  });
});
