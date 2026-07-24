// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {PrivacyGuideCompletionFragmentElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyGuideInteractions, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

/** Fire a sign in status change event and flush the UI. */
function setSignInState(signedIn: boolean) {
  const event = {
    signedIn: signedIn,
  };
  webUIListenerCallback('update-sync-state', event);
  flush();
}

suite('CompletionFragment', function() {
  let fragment: PrivacyGuideCompletionFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showAiPage: true,
    });
    resetRouterForTesting();
  });

  setup(function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    createPage();
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);
    return flushTasks();
  }

  teardown(function() {
    fragment.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('backNavigation', async function() {
    const nextEventPromise = eventToPromise('back-button-click', fragment);

    fragment.$.backButton.click();

    // Ensure the event is sent.
    return nextEventPromise;
  });

  test('backToSettingsNavigation', async function() {
    const closeEventPromise = eventToPromise('close', fragment);

    const leaveButton =
        fragment.shadowRoot!.querySelector<HTMLElement>('#leaveButton');
    assertTrue(!!leaveButton);
    leaveButton.click();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCompletion');

    // Ensure the |close| event has been sent.
    return closeEventPromise;
  });

  test('SWAALinkClick', async function() {
    setSignInState(true);

    const waaRow = fragment.shadowRoot!.querySelector<HTMLElement>('#waaRow');
    assertTrue(!!waaRow);
    assertTrue(isVisible(waaRow));
    waaRow.click();
    flush();

    assertEquals(
        PrivacyGuideInteractions.SWAA_COMPLETION_LINK,
        await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideEntryExitHistogram'));
    assertEquals(
        'Settings.PrivacyGuide.CompletionSWAAClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        loadTimeData.getString('activityControlsUrlInPrivacyGuide'),
        await openWindowProxy.whenCalled('openUrl'));
  });



  test('aiSettingsLink', async function() {
    const aiRow = fragment.shadowRoot!.querySelector<HTMLElement>('#aiRow');
    assertTrue(!!aiRow);
    assertTrue(isVisible(aiRow));
    aiRow.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.AI_SETTINGS_COMPLETION_LINK, result);
    assertEquals(
        'Settings.PrivacyGuide.CompletionAiSettingsClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertTrue(isChildVisible(fragment, '#aiRow'));
    assertTrue(isChildVisible(fragment, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(fragment, '#aiRow'));
    assertFalse(isChildVisible(fragment, '#waaRow'));
  });

  test('aiRowNotShownWhenAiPageHidden', function() {
    loadTimeData.overrideValues({
      showAiPage: false,
    });
    createPage();

    assertFalse(isChildVisible(fragment, '#aiRow'));
  });
});

