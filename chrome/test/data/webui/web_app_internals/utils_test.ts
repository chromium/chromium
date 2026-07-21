// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DebugData} from 'chrome://web-app-internals/web_app_internals_utils.js';
import {debugDataJsonReplacer, filterToApp, getAppIndexEntries, getQuery} from 'chrome://web-app-internals/web_app_internals_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// Tests only exercise the InstalledWebApps section. Cast as DebugData since
// other required sections are not relevant for these tests.
function makeFakeData(
    index: Record<string, string|string[]>,
    details: Array<{'!app_id': string, [key: string]: string}>): DebugData {
  return {
    InstalledWebApps: {
      '!Index': index,
      Details: details,
    },
  } as DebugData;
}

suite('WebAppInternalsUtilsTest', function() {
  suite('getQuery', function() {
    const originalHash = document.location.hash;

    teardown(function() {
      // Restore hash without triggering hashchange by using replaceState.
      history.replaceState(null, '', originalHash || '');
    });

    test('returns empty string when no hash', function() {
      history.replaceState(null, '', '');
      assertEquals('', getQuery());
    });

    test('returns fragment after hash', function() {
      history.replaceState(null, '', '#my-app-id');
      assertEquals('my-app-id', getQuery());
    });

    test('returns empty string for bare hash', function() {
      history.replaceState(null, '', '#');
      assertEquals('', getQuery());
    });
  });

  suite('filterToApp', function() {
    test('returns matching app details only', function() {
      const data = makeFakeData({'App1': 'id1', 'App2': 'id2'}, [
        {'!app_id': 'id1', 'name': 'App1'},
        {'!app_id': 'id2', 'name': 'App2'},
      ]);
      const result = filterToApp(data, 'id1');
      const installed = result.InstalledWebApps;
      assertEquals(1, installed.Details.length);
      assertEquals('id1', installed.Details[0]!['!app_id']);
    });

    test('preserves !Index', function() {
      const data =
          makeFakeData({'App1': 'id1'}, [{'!app_id': 'id1', 'name': 'App1'}]);
      const result = filterToApp(data, 'id1');
      const installed = result.InstalledWebApps;
      assertDeepEquals({'App1': 'id1'}, installed['!Index']);
    });

    test('returns full data when no app matches', function() {
      const data =
          makeFakeData({'App1': 'id1'}, [{'!app_id': 'id1', 'name': 'App1'}]);
      const result = filterToApp(data, 'nonexistent');
      assertEquals(data, result);
    });

    test('does not mutate the original data', function() {
      const data = makeFakeData({'App1': 'id1', 'App2': 'id2'}, [
        {'!app_id': 'id1', 'name': 'App1'},
        {'!app_id': 'id2', 'name': 'App2'},
      ]);
      filterToApp(data, 'id1');
      // Original data should still have both apps.
      const installed = data.InstalledWebApps;
      assertEquals(2, installed.Details.length);
    });
  });

  suite('appIndexUtils', function() {
    test('extracts entries with Show All prepended', function() {
      const data = makeFakeData({'App1': 'id1', 'App2': 'id2'}, []);
      const entries = getAppIndexEntries(data, 'id2');
      assertEquals(3, entries.length);
      assertDeepEquals(
          {id: '', label: 'Show All', isActive: false}, entries[0]);
      assertDeepEquals(
          {id: 'id1', label: 'App1 (id1)', isActive: false}, entries[1]);
      assertDeepEquals(
          {id: 'id2', label: 'App2 (id2)', isActive: true}, entries[2]);
    });

    test('marks Show All active when query is empty', function() {
      const data = makeFakeData({'App1': 'id1'}, []);
      const entries = getAppIndexEntries(data, '');
      assertTrue(entries[0]!.isActive);
      assertFalse(entries[1]!.isActive);
    });

    test('canonical key ordering', function() {
      const input = {
        IconErrorLog: [],
        InstalledWebApps: {'!Index': {}, Details: []},
        LockManager: {},
      };
      const json = JSON.stringify(input, debugDataJsonReplacer, 2);
      const keys = Object.keys(JSON.parse(json));
      assertDeepEquals(
          ['InstalledWebApps', 'LockManager', 'IconErrorLog'], keys);
    });
  });
});
