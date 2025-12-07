// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl} from 'chrome://history/history.js';
import type {PageRemote} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('GoogleAccountFooter', function() {
  let app: HistoryAppElement;
  let testService: TestBrowserService;
  let callbackRouterRemote: PageRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    callbackRouterRemote =
        testService.callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function createApp() {
    app = document.createElement('history-app');
    document.body.appendChild(app);
    return testService.handler.whenCalled('queryHistory');
  }

  // Simulate the browser notifying the page about other forms of history
  // changes.
  async function callOnHasOtherFormsChanged(hasChanged: boolean) {
    callbackRouterRemote.onHasOtherFormsChanged(hasChanged);
    await callbackRouterRemote.$.flushForTesting();
    await flushTasks();
  }

  function getGoogleAccountFooter() {
    const sidebar = app.$['contentSideBar'];
    const googleAccountFooter =
        sidebar.shadowRoot.querySelector<HTMLElement>('#google-account-footer');
    assertTrue(!!googleAccountFooter);
    return googleAccountFooter;
  }

  function isGoogleAccountFooterVisible() {
    const googleAccountFooter = getGoogleAccountFooter();
    return !googleAccountFooter.hidden;
  }

  function getGoogleAccountFooterMessageHTML() {
    const googleAccountFooter = getGoogleAccountFooter();

    const divElement = googleAccountFooter.querySelector('div:not([hidden])');
    assertTrue(!!divElement);

    // Make sure other div elements are hidden.
    const divHiddenElements =
        googleAccountFooter.querySelectorAll('div[hidden]');
    assertEquals(divHiddenElements.length, 2);

    return divElement.textContent;
  }

  function clickGoogleAccountFooterLinkWithId(id: string) {
    const googleAccountFooter = getGoogleAccountFooter();

    const divElement = googleAccountFooter.querySelector('div:not([hidden])');
    assertTrue(!!divElement);

    const linkElement = divElement.querySelector<HTMLAnchorElement>(`a#${id}`);
    assertTrue(!!linkElement);

    linkElement.click();
  }

  test('Neither My Activity nor Gemini Apps Activity visible', async () => {
    await createApp();
    assertFalse(isGoogleAccountFooterVisible());
  });

  test('Only My Activity visible', async () => {
    await createApp();
    await callOnHasOtherFormsChanged(true);

    assertTrue(isGoogleAccountFooterVisible());

    const expectedGmaOnlyMessage =
        'Your Google Account may have other forms of ' +
        'browsing history at myactivity.google.com';
    assertEquals(getGoogleAccountFooterMessageHTML(), expectedGmaOnlyMessage);

    // Verify that metric is recorded when the link is clicked and the correct
    // URL is opened.
    clickGoogleAccountFooterLinkWithId('footerGoogleMyActivityLink');
    assertEquals(
        1, testService.actionMap['SideBarFooterGoogleMyActivityClick']);

    const url = await testService.whenCalled('navigateToUrl');
    assertEquals(
        'https://myactivity.google.com/myactivity/?utm_source=chrome_h', url);
  });

  test('Only Gemini Apps Activity visible', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
      isGlicEnabled: true,
      enableBrowsingHistoryActorIntegrationM1: true,
    });
    await createApp();

    assertTrue(isGoogleAccountFooterVisible());

    const expectedGaaOnlyMessage =
        'Your Google Account may have your Gemini Apps Activity';
    assertEquals(getGoogleAccountFooterMessageHTML(), expectedGaaOnlyMessage);

    // Verify that metric is recorded when the link is clicked and the correct
    // URL is opened.
    clickGoogleAccountFooterLinkWithId('footerGeminiAppsActivityLink');
    assertEquals(
        1, testService.actionMap['SideBarFooterGeminiAppsActivityClick']);

    const url = await testService.whenCalled('navigateToUrl');
    assertEquals('https://myactivity.google.com/product/gemini', url);
  });

  test('Both My Activity and Gemini Apps Activity visible', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
      isGlicEnabled: true,
      enableBrowsingHistoryActorIntegrationM1: true,
    });
    await createApp();
    await callOnHasOtherFormsChanged(true);

    assertTrue(isGoogleAccountFooterVisible());

    const expectedGmaAndGaaMessage =
        'Your Google Account may have other forms of browsing history at ' +
        'myactivity.google.com, such as your Gemini Apps Activity';
    assertEquals(getGoogleAccountFooterMessageHTML(), expectedGmaAndGaaMessage);

    // Verify that metric is recorded when the link is clicked and the correct
    // URLs are opened.
    clickGoogleAccountFooterLinkWithId('footerGoogleMyActivityLink');
    assertEquals(
        1, testService.actionMap['SideBarFooterGoogleMyActivityClick']);

    const gma_url = await testService.whenCalled('navigateToUrl');
    assertEquals(
        'https://myactivity.google.com/myactivity/?utm_source=chrome_h',
        gma_url);
    testService.resetResolver('navigateToUrl');

    clickGoogleAccountFooterLinkWithId('footerGeminiAppsActivityLink');
    assertEquals(
        1, testService.actionMap['SideBarFooterGeminiAppsActivityClick']);

    const gaa_url = await testService.whenCalled('navigateToUrl');
    assertEquals('https://myactivity.google.com/product/gemini', gaa_url);
  });

  test('Gemini Apps Activity hidden when glic disabled', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
      isGlicEnabled: false,
      enableBrowsingHistoryActorIntegrationM1: true,
    });
    await createApp();

    assertFalse(isGoogleAccountFooterVisible());
  });

  test('Gemini Apps Activity hidden when feature flag disabled', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
      isGlicEnabled: true,
      enableBrowsingHistoryActorIntegrationM1: false,
    });
    await createApp();

    assertFalse(isGoogleAccountFooterVisible());
  });
});
