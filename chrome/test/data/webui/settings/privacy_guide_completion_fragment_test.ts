// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {PrivacyGuideCompletionFragmentElement} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement} from 'chrome://settings/settings.js';
import {loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyGuideBrowserProxyImpl, PrivacyGuideInteractions, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyGuideBrowserProxy} from './test_privacy_guide_browser_proxy.js';

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
  let testPrivacyGuideBrowserProxy: TestPrivacyGuideBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyGuideBrowserProxy = new TestPrivacyGuideBrowserProxy();
    testPrivacyGuideBrowserProxy
        .setPrivacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel(
            false);
    PrivacyGuideBrowserProxyImpl.setInstance(testPrivacyGuideBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

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

    fragment.shadowRoot!.querySelector<HTMLElement>('#leaveButton')!.click();

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

    assertTrue(isChildVisible(fragment, '#waaRow'));
    fragment.shadowRoot!.querySelector<HTMLElement>('#waaRow')!.click();
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

  test('privacySandboxLink', async function() {
    const privacySandboxRow =
        fragment.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxRow');
    assertTrue(!!privacySandboxRow);
    assertEquals(
        fragment.i18n('privacyGuideCompletionCardPrivacySandboxSubLabel'),
        privacySandboxRow.subLabel);
    privacySandboxRow!.click();
    flush();

    assertEquals(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK,
        await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideEntryExitHistogram'));
    assertEquals(
        'Settings.PrivacyGuide.CompletionPSClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(fragment, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(fragment, '#waaRow'));
  });
});

suite('CompletionFragmentPrivacySandboxRestricted', function() {
  let fragment: PrivacyGuideCompletionFragmentElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

  teardown(function() {
    fragment.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('waaRowShownWhenSignedIn', function() {
    setSignInState(true);
    assertFalse(isChildVisible(fragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(fragment, '#waaRow'));
    const subheader =
        fragment.shadowRoot!.querySelector<HTMLElement>('.cr-secondary-text');
    assertTrue(!!subheader);
    assertEquals(
        fragment.i18n('privacyGuideCompletionCardSubHeader'),
        subheader.innerText);
  });

  test('noLinksShownWhenSignedOut', function() {
    setSignInState(false);
    assertFalse(isChildVisible(fragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(fragment, '#waaRow'));
    const subheader =
        fragment.shadowRoot!.querySelector<HTMLElement>('.cr-secondary-text');
    assertTrue(!!subheader);
    assertEquals(
        fragment.i18n('privacyGuideCompletionCardSubHeaderNoLinks'),
        subheader.innerText);
  });
});

suite(
    'CompletionFragmentPrivacySandboxRestrictedWithNoticeEnabled', function() {
      let fragment: PrivacyGuideCompletionFragmentElement;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          isPrivacySandboxRestricted: true,
          isPrivacySandboxRestrictedNoticeEnabled: true,
        });
        resetRouterForTesting();
      });

      setup(function() {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;

        assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
        fragment = document.createElement('privacy-guide-completion-fragment');
        document.body.appendChild(fragment);

        return flushTasks();
      });

      teardown(function() {
        fragment.remove();
        // The browser instance is shared among the tests, hence the route needs
        // to be reset between tests.
        Router.getInstance().navigateTo(routes.BASIC);
      });

      test('privacySandboxRowVisibility', function() {
        assertTrue(isChildVisible(fragment, '#privacySandboxRow'));
      });
    });

suite('CompletionFragmentWithAdTopicsCard', function() {
  let fragment: PrivacyGuideCompletionFragmentElement;
  let testPrivacyGuideBrowserProxy: TestPrivacyGuideBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testPrivacyGuideBrowserProxy = new TestPrivacyGuideBrowserProxy();
    testPrivacyGuideBrowserProxy
        .setPrivacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel(
            true);
    PrivacyGuideBrowserProxyImpl.setInstance(testPrivacyGuideBrowserProxy);
    fragment = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(fragment);

    return flushTasks();
  });

  test('TestAdTopicsCrLinkRowSubLabel', function() {
    const privacySandboxRow =
        fragment.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxRow');
    assertTrue(!!privacySandboxRow);
    assertEquals(
        fragment.i18n(
            'privacyGuideCompletionCardPrivacySandboxSubLabelAdTopics'),
        privacySandboxRow.subLabel);
  });
});
