// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

async function createAndAppendRealbox(
    properties: Partial<NtpSearchboxElement> = {}):
    Promise<NtpSearchboxElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('ntp-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  await microtasksFinished();
  return realbox;
}

// Returns the "+" context menu element and its inner #entrypoint button.
async function getContextMenuAndEntrypoint(realbox: NtpSearchboxElement) {
  const contextElement = realbox.shadowRoot.querySelector(
      'cr-composebox-contextual-entrypoint-and-menu');
  assertTrue(!!contextElement);
  await contextElement.updateComplete;
  const entrypointButton = contextElement.shadowRoot.querySelector(
      'cr-composebox-contextual-entrypoint-button');
  assertTrue(!!entrypointButton);
  await entrypointButton.updateComplete;
  const entrypoint =
      entrypointButton.shadowRoot.querySelector<HTMLElement>('#entrypoint');
  assertTrue(!!entrypoint);
  return {contextElement, entrypoint};
}

suite('SearchboxFocusTest', () => {
  let testProxy: TestSearchboxBrowserProxy;
  const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);

  setup(() => {
    loadTimeData.overrideValues({
      isLensSearchbox: false,
      searchboxCyclingPlaceholders: false,
      searchboxDefaultIcon: 'search.svg',
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
      reportMetrics: true,
    });

    // Set up Realbox's browser proxy.
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    // Set up MetricsReporter's browser proxy.
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());
  });

  test('searchbox renders', async () => {
    await createAndAppendRealbox();
    const realbox = document.querySelector('ntp-searchbox');
    assertTrue(!!realbox);
  });

  suite('ContextMenuFocus', () => {
    let realbox: NtpSearchboxElement;

    setup(async () => {
      realbox = await createAndAppendRealbox(
          {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});
    });

    // Regression test for b/564743397: after the "+" menu is used with
    // the keyboard, focus must stay on "+" so that it is restored there when a
    // dialog opened from the menu (e.g. the Drive sign-in promo) closes.
    test('keyboard focus stays on "+" when its menu closes', async () => {
      const {contextElement, entrypoint} =
          await getContextMenuAndEntrypoint(realbox);

      // The menu returns focus to "+" on close; simulate keyboard modality.
      FocusOutlineManager.forDocument(document).visible = true;
      entrypoint.focus({focusVisible: true});
      assertEquals(entrypoint, getDeepActiveElement());
      assertTrue(entrypoint.matches(':focus-visible'));

      contextElement.dispatchEvent(new CustomEvent('context-menu-closed'));
      await microtasksFinished();

      assertEquals(entrypoint, getDeepActiveElement());
      assertTrue(entrypoint.matches(':focus-visible'));
      assertDeepEquals(
          [{focused: true}], testProxy.handler.getArgs('onFocusChanged'));
    });

    // After mouse use, focus is dropped so #searchboxContainer drops its scrim
    // (https://crrev.com/c/7540901).
    test('mouse focus is dropped when "+" menu closes', async () => {
      const {contextElement, entrypoint} =
          await getContextMenuAndEntrypoint(realbox);

      // The menu returns focus to "+" on close; simulate mouse modality.
      FocusOutlineManager.forDocument(document).visible = false;
      entrypoint.focus({focusVisible: false});
      assertEquals(entrypoint, getDeepActiveElement());
      assertFalse(entrypoint.matches(':focus-visible'));

      contextElement.dispatchEvent(new CustomEvent('context-menu-closed'));
      await microtasksFinished();

      assertEquals(document.body, getDeepActiveElement());
      assertDeepEquals(
          [{focused: true}, {focused: false}],
          testProxy.handler.getArgs('onFocusChanged'));
    });

    // Runs the real close sequence: <cr-action-menu> returns focus to "+", then
    // 'context-menu-closed' fires on a later animation frame.
    test('keyboard focus stays on "+" after the real menu closes', async () => {
      const {contextElement, entrypoint} =
          await getContextMenuAndEntrypoint(realbox);
      FocusOutlineManager.forDocument(document).visible = true;
      entrypoint.focus({focusVisible: true});

      const whenOpened = eventToPromise('context-menu-opened', contextElement);
      entrypoint.click();
      await whenOpened;
      const crActionMenu = contextElement.$.menu.$.menu;
      assertTrue(crActionMenu.open);

      const whenClosed = eventToPromise('context-menu-closed', contextElement);
      keyDownOn(crActionMenu, 0, [], 'Escape');
      await whenClosed;
      await microtasksFinished();

      assertFalse(crActionMenu.open);
      assertTrue(FocusOutlineManager.forDocument(document).visible);
      assertEquals(entrypoint, getDeepActiveElement());
      assertTrue(entrypoint.matches(':focus-visible'));
      assertDeepEquals(
          [{focused: true}], testProxy.handler.getArgs('onFocusChanged'));
    });

    // Even after earlier keyboard use on the page, closing the real menu with a
    // mouse click on the backdrop drops focus from "+".
    test(
        'mouse backdrop click drops focus after real menu closes', async () => {
          const {contextElement, entrypoint} =
              await getContextMenuAndEntrypoint(realbox);
          FocusOutlineManager.forDocument(document).visible = true;

          const whenOpened =
              eventToPromise('context-menu-opened', contextElement);
          // A synthetic mousedown has no default action, so focus "+" the way
          // a real mouse press does before the click opens the menu.
          entrypoint.dispatchEvent(
              new MouseEvent('mousedown', {bubbles: true, composed: true}));
          entrypoint.focus({focusVisible: false});
          entrypoint.click();
          await whenOpened;
          const crActionMenu = contextElement.$.menu.$.menu;
          assertTrue(crActionMenu.open);

          const whenClosed =
              eventToPromise('context-menu-closed', contextElement);
          const dialog = crActionMenu.getDialog();
          dialog.dispatchEvent(
              new MouseEvent('mousedown', {bubbles: true, composed: true}));
          dialog.click();
          await whenClosed;
          await microtasksFinished();

          assertFalse(crActionMenu.open);
          assertFalse(FocusOutlineManager.forDocument(document).visible);
          assertEquals(document.body, getDeepActiveElement());
          assertDeepEquals(
              [{focused: true}, {focused: false}],
              testProxy.handler.getArgs('onFocusChanged'));
        });
  });
});
