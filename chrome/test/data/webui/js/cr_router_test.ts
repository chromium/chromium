// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

suite('CrRouterTest', function() {
  setup(function() {
    // Clear out the URL state, for a clean start.
    window.history.replaceState({}, '', '/');
    window.dispatchEvent(new CustomEvent('popstate'));
  });

  test('sets url from setters', function() {
    const router = CrRouter.getInstance();

    assertEquals('/', window.location.pathname);
    router.setPath('foo/nested');
    assertEquals('/foo/nested', window.location.pathname);

    assertEquals('', window.location.hash);
    router.setHash('bar');
    assertEquals('#bar', window.location.hash);

    const queryParams =
        new URLSearchParams(window.location.search.substring(1));
    assertFalse(queryParams.has('q'));
    queryParams.set('q', 'hello world');
    router.setQueryParams(queryParams);
    const updatedParams =
        new URLSearchParams(window.location.search.substring(1));
    assertEquals('hello world', updatedParams.get('q'));
  });

  test('calls listeners when url changes', function() {
    const pathHistory: string[] = [];
    const queryHistory: URLSearchParams[] = [];
    const hashHistory: string[] = [];
    const pathListener = (e: Event) =>
        pathHistory.push((e as CustomEvent<string>).detail);
    const queryListener = (e: Event) =>
        queryHistory.push((e as CustomEvent<URLSearchParams>).detail);
    const hashListener = (e: Event) =>
        hashHistory.push((e as CustomEvent<string>).detail);

    const router = CrRouter.getInstance();
    assertEquals('/', router.getPath());
    assertEquals('', router.getQueryParams().toString());
    assertEquals('', router.getHash());

    router.addEventListener('cr-router-path-changed', pathListener);
    router.addEventListener('cr-router-query-params-changed', queryListener);
    router.addEventListener('cr-router-hash-changed', hashListener);

    assertEquals(0, pathHistory.length);
    assertEquals(0, queryHistory.length);
    assertEquals(0, hashHistory.length);

    window.history.replaceState(
        {}, '', '/foo/nested?q=hello+world&test=def#bar');
    window.dispatchEvent(new CustomEvent('popstate'));
    assertEquals(1, pathHistory.length);
    assertEquals(1, queryHistory.length);
    assertEquals(1, hashHistory.length);
    assertEquals('/foo/nested', pathHistory[0]!);
    assertEquals('q=hello+world&test=def', queryHistory[0]!.toString());
    assertEquals('hello world', queryHistory[0]!.get('q'));
    assertEquals('bar', hashHistory[0]!);

    router.removeEventListener('cr-router-path-changed', pathListener);
    router.removeEventListener('cr-router-query-params-changed', queryListener);
    router.removeEventListener('cr-router-hash-changed', hashListener);
  });
});
