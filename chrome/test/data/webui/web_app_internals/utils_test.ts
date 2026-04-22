// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DebugSection, InstalledWebAppsData} from 'chrome://web-app-internals/web_app_internals_utils.js';
import {filterToApp, getQuery, renderAppIndex} from 'chrome://web-app-internals/web_app_internals_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

function getInstalledSection(section: DebugSection): InstalledWebAppsData {
  return section['InstalledWebApps'] as InstalledWebAppsData;
}

function makeFakeData(
    index: Record<string, string|string[]>,
    details: Array<Record<string, string>>): DebugSection[] {
  return [
    {'OtherSection': {'key': 'value'}},
    {
      'InstalledWebApps': {
        '!Index': index,
        'Details': details,
      },
    },
  ];
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
      assertEquals(1, result.length);
      const installed = getInstalledSection(result[0]!);
      assertEquals(1, installed.Details.length);
      assertEquals('id1', installed.Details[0]!['!app_id']);
    });

    test('preserves !Index', function() {
      const data =
          makeFakeData({'App1': 'id1'}, [{'!app_id': 'id1', 'name': 'App1'}]);
      const result = filterToApp(data, 'id1');
      const installed = getInstalledSection(result[0]!);
      assertDeepEquals({'App1': 'id1'}, installed['!Index']);
    });

    test('returns full data when InstalledWebApps section missing', function() {
      const data: DebugSection[] = [{'OtherSection': {}}];
      const result = filterToApp(data, 'id1');
      assertDeepEquals(data, result);
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
      const installed = getInstalledSection(data[1]!);
      assertEquals(2, installed.Details.length);
    });
  });

  suite('renderAppIndex', function() {
    let container: HTMLElement;

    setup(function() {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      container = document.createElement('div');
      document.body.appendChild(container);
    });

    test('creates links from Index data', function() {
      const data = makeFakeData({'App1': 'id1', 'App2': 'id2'}, []);
      renderAppIndex(data, container, '');
      const links = container.querySelectorAll('a');
      // "Show All" + 2 app links
      assertEquals(3, links.length);
      assertEquals('App1 (id1)', links[1]!.textContent);
      assertEquals('#id1', links[1]!.getAttribute('href'));
      assertEquals('App2 (id2)', links[2]!.textContent);
      assertEquals('#id2', links[2]!.getAttribute('href'));
    });

    test('includes Show All link', function() {
      const data = makeFakeData({'App1': 'id1'}, []);
      renderAppIndex(data, container, '');
      const links = container.querySelectorAll('a');
      assertEquals('Show All', links[0]!.textContent);
      assertEquals('#', links[0]!.getAttribute('href'));
    });

    test('handles array of IDs for same app name', function() {
      const data = makeFakeData({'App1': ['id1', 'id2']}, []);
      renderAppIndex(data, container, '');
      const links = container.querySelectorAll('a');
      // "Show All" + 2 links for same app name
      assertEquals(3, links.length);
      assertEquals('App1 (id1)', links[1]!.textContent);
      assertEquals('App1 (id2)', links[2]!.textContent);
    });

    test('handles missing InstalledWebApps section', function() {
      const data: DebugSection[] = [{'OtherSection': {}}];
      renderAppIndex(data, container, '');
      assertEquals(0, container.children.length);
    });

    test('marks matching link as active', function() {
      const data = makeFakeData({'App1': 'id1', 'App2': 'id2'}, []);
      renderAppIndex(data, container, 'id2');
      const links = container.querySelectorAll('a');
      assertFalse(links[0]!.classList.contains('active'));
      assertFalse(links[1]!.classList.contains('active'));
      assertTrue(links[2]!.classList.contains('active'));
    });

    test('marks Show All as active when query is empty', function() {
      const data = makeFakeData({'App1': 'id1'}, []);
      renderAppIndex(data, container, '');
      const links = container.querySelectorAll('a');
      assertTrue(links[0]!.classList.contains('active'));
      assertFalse(links[1]!.classList.contains('active'));
    });

    test('marks Show All as active when query matches no app', function() {
      const data = makeFakeData({'App1': 'id1'}, []);
      renderAppIndex(data, container, 'nonexistent');
      const links = container.querySelectorAll('a');
      assertTrue(links[0]!.classList.contains('active'));
      assertFalse(links[1]!.classList.contains('active'));
    });

    test('clears previous content', function() {
      container.appendChild(document.createElement('span'));
      const data = makeFakeData({'App1': 'id1'}, []);
      renderAppIndex(data, container, '');
      assertEquals(0, container.querySelectorAll('span').length);
      assertEquals(2, container.querySelectorAll('a').length);
    });
  });
});
