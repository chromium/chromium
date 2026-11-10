// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {whenCheck} from 'chrome://webui-test/test_util.js';

suite('OptimizationGuideInternalsTest', function() {
  test('EmptyTest', function() {
    // Nothing to do here. This is to support a test that checks the state of
    // the logger before and after opening the page.
  });

  test('InternalsPageOpen', function() {
    const container = document.getElementById('log-message-container');
    assertTrue(!!container);
    return whenCheck(
        container, () => (container.children[0]?.childElementCount ?? 0) > 2);
  });

  test('InternalsModelsPageOpen', function() {
    window.history.replaceState({}, '', '#models');
    window.dispatchEvent(new CustomEvent('hashchange'));

    const container = document.getElementById('downloaded-models-container');
    assertTrue(!!container);
    const containerHasChildren = whenCheck(
        container, () => (container.children[0]?.childElementCount ?? 0) > 0);
    const tableRowExists = whenCheck(
        container,
        () =>
            !!document.getElementById('OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD'));

    return Promise.all([containerHasChildren, tableRowExists]);
  });

  test('InternalsClientIdsPageOpen', function() {
    window.history.replaceState({}, '', '#client-ids');
    window.dispatchEvent(new CustomEvent('hashchange'));

    const container = document.getElementById('logged-client-ids-container');
    assertTrue(!!container);
    return whenCheck(
        container, () => (container.children[0]?.childElementCount ?? 0) > 0);
  });

  test('InternalsMqlsLogsPageOpen', async function() {
    window.history.replaceState({}, '', '#mqls-logs');
    window.dispatchEvent(new CustomEvent('hashchange'));

    const container = document.getElementById('mqls-logs-container');
    assertTrue(!!container);
    await whenCheck(container, () => {
      if ((container.children[0]?.childElementCount ?? 0) === 0) {
        return false;
      }
      const rows = container.querySelectorAll<HTMLTableRowElement>('tr');
      // Header row + body row
      return rows.length === 2;
    });

    const rows = container.querySelectorAll<HTMLTableRowElement>('tr');
    assertEquals(2, rows.length);
    const cells = rows[1]!.querySelectorAll<HTMLTableCellElement>('td');
    assertEquals('Compose', cells[0]!.innerText);
    assertStringContains(atob(cells[1]!.innerText), 'a user typed this');
    assertStringContains(atob(cells[1]!.innerText), 'compose response');
    assertEquals('Not allowed to upload', cells[2]!.innerText);
  });

  test('FilterUrlParamsReflection', function() {
    const includeInput =
        document.getElementById('log-message-include') as HTMLInputElement;
    const excludeInput =
        document.getElementById('log-message-exclude') as HTMLInputElement;
    assertTrue(!!includeInput);
    assertTrue(!!excludeInput);

    includeInput.value = 'test_include';
    includeInput.dispatchEvent(new Event('input'));
    let params = new URLSearchParams(window.location.search);
    assertEquals('test_include', params.get('include'));

    excludeInput.value = 'test_exclude';
    excludeInput.dispatchEvent(new Event('input'));
    params = new URLSearchParams(window.location.search);
    assertEquals('test_include', params.get('include'));
    assertEquals('test_exclude', params.get('exclude'));

    includeInput.value = '';
    includeInput.dispatchEvent(new Event('input'));
    params = new URLSearchParams(window.location.search);
    assertFalse(params.has('include'));
    assertEquals('test_exclude', params.get('exclude'));

    excludeInput.value = '';
    excludeInput.dispatchEvent(new Event('input'));
    params = new URLSearchParams(window.location.search);
    assertFalse(params.has('include'));
    assertFalse(params.has('exclude'));
  });

  test('FilterUrlParamsPopstate', function() {
    const includeInput =
        document.getElementById('log-message-include') as HTMLInputElement;
    const excludeInput =
        document.getElementById('log-message-exclude') as HTMLInputElement;
    assertTrue(!!includeInput);
    assertTrue(!!excludeInput);

    window.history.pushState(null, '', '?include=hist_inc&exclude=hist_exc');
    window.dispatchEvent(new PopStateEvent('popstate'));
    assertEquals('hist_inc', includeInput.value);
    assertEquals('hist_exc', excludeInput.value);

    window.history.pushState(null, '', window.location.pathname);
    window.dispatchEvent(new PopStateEvent('popstate'));
    assertEquals('', includeInput.value);
    assertEquals('', excludeInput.value);
  });

  test('FilterUrlParamsInitial', function() {
    const includeInput =
        document.getElementById('log-message-include') as HTMLInputElement;
    const excludeInput =
        document.getElementById('log-message-exclude') as HTMLInputElement;
    assertTrue(!!includeInput);
    assertTrue(!!excludeInput);
    assertEquals('init_inc', includeInput.value);
    assertEquals('init_exc', excludeInput.value);
  });
});
