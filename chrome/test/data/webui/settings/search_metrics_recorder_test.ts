// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {MAX_DWELL_TIME_MS, MetricsBrowserProxyImpl, QUERY_SUBMITTED_DELAY_MS, Router, routes, SearchMetricsRecorder, SettingsSearchExitReasonDesktop} from 'chrome://settings/settings.js';
import {assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
// clang-format on

/** Simulates the user typing `query` in the search box. */
function search(query: string) {
  Router.getInstance().navigateTo(
      routes.BASIC, new URLSearchParams(`search=${query}`));
}

/** Simulates the user clearing the search box. */
function exitSearch() {
  Router.getInstance().navigateTo(
      routes.BASIC, /* dynamicParams= */ undefined, /* removeSearch= */ true);
}

suite('SearchMetricsRecorder', function() {
  let recorder: SearchMetricsRecorder;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let mockTimer: MockTimer;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    mockTimer = new MockTimer();
    mockTimer.install();

    exitSearch();
    recorder = new SearchMetricsRecorder();
    recorder.start();
  });

  teardown(function() {
    recorder.stop();
    mockTimer.uninstall();
    exitSearch();
  });

  /**
   * Simulates the user leaving the query alone for long enough that it counts
   * as submitted. Depends on the suite's MockTimer.
   */
  function simulateQuerySubmitted() {
    mockTimer.tick(QUERY_SUBMITTED_DELAY_MS);
  }

  test('records submitted queries only once the user stops typing', function() {
    search('f');
    search('fo');
    search('foo');
    assertEquals(0, metricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        0, metricsBrowserProxy.getCallCount('recordSettingsSearchQueryEntered'));

    simulateQuerySubmitted();
    assertEquals(1, metricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.Search.QuerySubmitted',
        metricsBrowserProxy.getArgs('recordAction')[0]);
    assertEquals(
        1, metricsBrowserProxy.getCallCount('recordSettingsSearchQueryEntered'));
  });

  test('records whether the submitted query had results', function() {
    search('foo');
    recorder.onSearchFinished('foo', 3);
    simulateQuerySubmitted();

    let args = metricsBrowserProxy.getArgs('recordBooleanHistogram')[0];
    assertEquals('Settings.Search.HasResults', args[0]);
    assertTrue(args[1]);

    metricsBrowserProxy.reset();

    search('bar');
    recorder.onSearchFinished('bar', 0);
    simulateQuerySubmitted();

    args = metricsBrowserProxy.getArgs('recordBooleanHistogram')[0];
    assertEquals('Settings.Search.HasResults', args[0]);
    assertEquals(false, args[1]);
  });

  test('waits for pending results before recording HasResults', function() {
    search('foo');
    simulateQuerySubmitted();
    assertEquals(0, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));

    // Results of an older query are ignored.
    recorder.onSearchFinished('fo', 5);
    assertEquals(0, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));

    recorder.onSearchFinished('foo', 5);
    assertEquals(1, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));
  });

  test('records clearing the query as an exit reason', async function() {
    search('foo');
    simulateQuerySubmitted();
    exitSearch();

    assertEquals(
        SettingsSearchExitReasonDesktop.CLEAR_QUERY,
        await metricsBrowserProxy.whenCalled('recordSettingsSearchExitReason'));
  });

  test('records sidebar navigations as an exit reason', async function() {
    search('foo');
    simulateQuerySubmitted();

    // The sidebar navigates to a section and drops the search query.
    Router.getInstance().navigateTo(
        routes.PEOPLE, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);

    assertEquals(
        SettingsSearchExitReasonDesktop.NAVIGATE_SIDEBAR,
        await metricsBrowserProxy.whenCalled('recordSettingsSearchExitReason'));
  });

  test('records back navigations as an exit reason', async function() {
    search('foo');
    simulateQuerySubmitted();

    // Browser back/forward navigations are routed through setCurrentRoute()
    // with isPopstate=true by the popstate listener in route.ts.
    Router.getInstance().setCurrentRoute(
        routes.PEOPLE, new URLSearchParams(), /* isPopstate= */ true);

    assertEquals(
        SettingsSearchExitReasonDesktop.BACK_NAVIGATION,
        await metricsBrowserProxy.whenCalled('recordSettingsSearchExitReason'));
  });

  test('records interacting with a result as an exit reason', async function() {
    search('foo');
    simulateQuerySubmitted();
    recorder.onSearchResultInteraction();

    assertEquals(
        SettingsSearchExitReasonDesktop.INTERACTED_WITH_RESULT,
        await metricsBrowserProxy.whenCalled('recordSettingsSearchExitReason'));
  });

  test('records interacting with a result only once', function() {
    search('foo');
    simulateQuerySubmitted();
    recorder.onSearchResultInteraction();

    // A single interaction can be reported more than once, since 'click' and
    // 'change' both fire for it, and opening a result then navigates while
    // keeping the query in the URL. All of that belongs to a session that has
    // already ended.
    recorder.onSearchResultInteraction();
    Router.getInstance().navigateTo(routes.PEOPLE);

    assertEquals(
        1, metricsBrowserProxy.getCallCount('recordSettingsSearchExitReason'));
  });

  test('records leaving the page as an exit reason', async function() {
    search('foo');
    simulateQuerySubmitted();
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(
        SettingsSearchExitReasonDesktop.EXIT_SETTINGS,
        await metricsBrowserProxy.whenCalled('recordSettingsSearchExitReason'));
  });

  test('ignores sessions abandoned before submitting a query', function() {
    search('foo');
    exitSearch();

    assertEquals(0, metricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        0, metricsBrowserProxy.getCallCount('recordSettingsSearchExitReason'));
  });

  test('records queries abandoned by interacting with the results', function() {
    search('foo');
    recorder.onSearchResultInteraction();

    // Acting on the results is a stronger signal than the debounce timer, so
    // the query still counts as submitted.
    assertEquals(
        'Settings.Search.QuerySubmitted',
        metricsBrowserProxy.getArgs('recordAction')[0]);
    assertEquals(
        SettingsSearchExitReasonDesktop.INTERACTED_WITH_RESULT,
        metricsBrowserProxy.getArgs('recordSettingsSearchExitReason')[0]);
  });

  test('records dwell time when returning to search', async function() {
    search('foo');
    simulateQuerySubmitted();
    recorder.onSearchResultInteraction();
    assertEquals(
        0,
        metricsBrowserProxy.getCallCount(
            'recordSettingsSearchResultDwellTime'));

    // Issuing a new query means the user came back from the result.
    search('bar');
    const dwellTime = await metricsBrowserProxy.whenCalled(
        'recordSettingsSearchResultDwellTime');
    assertGE(dwellTime, 0);
  });

  test('drops dwell times longer than the maximum', function() {
    search('foo');
    simulateQuerySubmitted();
    recorder.onSearchResultInteraction();

    // Dwell times are measured with Date.now(), which MockTimer does not
    // control, so move the clock forward explicitly.
    const realDateNow = Date.now;
    Date.now = () => realDateNow() + MAX_DWELL_TIME_MS + 1;
    try {
      search('bar');
    } finally {
      Date.now = realDateNow;
    }

    assertEquals(
        0,
        metricsBrowserProxy.getCallCount(
            'recordSettingsSearchResultDwellTime'));
  });

  test('skips HasResults when interacting before results arrive', function() {
    search('foo');
    simulateQuerySubmitted();

    // The user interacts with a result while the search request is still
    // running, so there is nothing to report for this query.
    recorder.onSearchResultInteraction();
    recorder.onSearchFinished('foo', 3);

    assertEquals(1, metricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(0, metricsBrowserProxy.getCallCount('recordBooleanHistogram'));
  });

  test('picks up the current query again after a restart', function() {
    search('foo');
    recorder.stop();
    metricsBrowserProxy.reset();

    // The URL still carries 'foo', so restarting has to start a new session
    // for it rather than treating it as a query that was already seen.
    recorder.start();
    simulateQuerySubmitted();

    assertEquals(
        'Settings.Search.QuerySubmitted',
        metricsBrowserProxy.getArgs('recordAction')[0]);
  });

  test('does not record metrics after being stopped', function() {
    recorder.stop();

    search('foo');
    simulateQuerySubmitted();
    exitSearch();

    assertEquals(0, metricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        0, metricsBrowserProxy.getCallCount('recordSettingsSearchExitReason'));
  });
});
