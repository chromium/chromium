// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ErrorPanelType, GlicOverlayBrowserProxyImpl, GlicOverlayPageCallbackRouter, GlicOverlayPageHandlerRemote, LoadingStyle} from 'chrome://glic/glic_overlay.js';
import type {GlicOverlayBrowserProxy, GlicOverlayElement, GlicOverlayPageRemote} from 'chrome://glic/glic_overlay.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

class TestGlicOverlayBrowserProxy implements GlicOverlayBrowserProxy {
  handler: TestMock<GlicOverlayPageHandlerRemote>&GlicOverlayPageHandlerRemote;
  router: GlicOverlayPageCallbackRouter;
  remote: GlicOverlayPageRemote;

  constructor() {
    this.handler = TestMock.fromClass(GlicOverlayPageHandlerRemote);
    this.router = new GlicOverlayPageCallbackRouter();
    this.remote = this.router.$.bindNewPipeAndPassRemote();
  }
}

suite('GlicOverlayTest', () => {
  let overlay: GlicOverlayElement;
  let browserProxy: TestGlicOverlayBrowserProxy;
  let overlayTemplate: HTMLElement;

  suiteSetup(() => {
    const initialOverlay = document.querySelector('glic-overlay');
    assertTrue(
        !!initialOverlay, 'initial glic-overlay element not found in DOM');
    overlayTemplate = initialOverlay.cloneNode(true) as HTMLElement;
  });

  setup(() => {
    document.body.className = '';
    document.body.replaceChildren();

    browserProxy = new TestGlicOverlayBrowserProxy();
    GlicOverlayBrowserProxyImpl.setInstance(browserProxy);

    overlay = overlayTemplate.cloneNode(true) as GlicOverlayElement;
    document.body.appendChild(overlay);
  });

  function getElement<T extends HTMLElement = HTMLElement>(id: string): T {
    const el = overlay.querySelector<T>(`#${id}`);
    assertTrue(!!el, `Element #${id} not found`);
    return el;
  }

  test('ShowLoadingFloating', async () => {
    browserProxy.remote.setOverlayState({loading: LoadingStyle.kFloating});
    await browserProxy.remote.$.flushForTesting();

    assertFalse(getElement('loadingPanel').hidden);
    assertTrue(document.body.classList.contains('floating'));
    assertFalse(document.body.classList.contains('sidePanel'));
  });

  test('ShowLoadingSidePanel', async () => {
    browserProxy.remote.setOverlayState({loading: LoadingStyle.kSidePanel});
    await browserProxy.remote.$.flushForTesting();

    assertFalse(getElement('loadingPanel').hidden);
    assertTrue(document.body.classList.contains('sidePanel'));
    assertFalse(document.body.classList.contains('floating'));
  });

  test('ShowErrorPanels', async () => {
    const errorTypes: Array<{type: ErrorPanelType, id: string}> = [
      {type: ErrorPanelType.kOffline, id: 'offlinePanel'},
      {type: ErrorPanelType.kError, id: 'errorPanel'},
      {type: ErrorPanelType.kUnavailable, id: 'unavailablePanel'},
      {type: ErrorPanelType.kIneligibleAccount, id: 'ineligibleAccountPanel'},
      {type: ErrorPanelType.kDisabledByAdmin, id: 'disabledByAdminPanel'},
      {
        type: ErrorPanelType.kDisabledByAdminWithLink,
        id: 'disabledByAdminPanel',
      },
      {type: ErrorPanelType.kSignIn, id: 'signInPanel'},
      {type: ErrorPanelType.kLocationMismatch, id: 'locationMismatchPanel'},
    ];

    const allPanels = Array.from(
        overlay.querySelectorAll<HTMLElement>('#localPanels .dialog.panel'));

    for (const {type, id} of errorTypes) {
      browserProxy.remote.setOverlayState({error: type});
      await browserProxy.remote.$.flushForTesting();

      const expectedPanel = getElement(id);
      assertFalse(expectedPanel.hidden, `Expected ${id} to be visible`);

      for (const panel of allPanels) {
        if (panel !== expectedPanel) {
          assertTrue(
              panel.hidden,
              `Expected ${panel.id} to be hidden when ${id} is active`);
        }
      }

      if (type === ErrorPanelType.kDisabledByAdminWithLink) {
        assertTrue(
            getElement(id).classList.contains('show-disabled-by-admin-link'));
      }
    }
  });

  test('ButtonClicksNotifyHandler', () => {
    const clickCases:
        Array<{id: string, method: keyof GlicOverlayPageHandlerRemote}> = [
          {id: 'retry', method: 'onRetryClicked'},
          {id: 'reload', method: 'onRetryClicked'},
          {id: 'signInButton', method: 'onSignInClicked'},
          {id: 'profilePickerButton', method: 'onProfilePickerClicked'},
          {
            id: 'ineligibleAccountHelpButton',
            method: 'onIneligibleAccountHelpClicked',
          },
          {
            id: 'locationMismatchHelpButton',
            method: 'onLocationMismatchHelpClicked',
          },
          {
            id: 'disabledByAdminCloseButton',
            method: 'onDisabledByAdminCloseClicked',
          },
        ];

    for (const {id, method} of clickCases) {
      const button = getElement(id);
      button.click();
      assertEquals(
          1, browserProxy.handler.getCallCount(method),
          `Expected call for ${method}`);
      browserProxy.handler.resetResolver(method);
    }

    const adminLink = getElement('disabledByAdminPanel').querySelector('a')!;
    adminLink.click();
    assertEquals(
        1, browserProxy.handler.getCallCount('onDisabledByAdminLinkClicked'));

    const closeButtons =
        Array.from(overlay.querySelectorAll<HTMLElement>('.close-button'));
    assertTrue(closeButtons.length > 0, 'Expected close buttons to exist');
    for (let i = 0; i < closeButtons.length; i++) {
      browserProxy.handler.resetResolver('onClosePanelClicked');
      closeButtons[i]!.click();
      assertEquals(
          1, browserProxy.handler.getCallCount('onClosePanelClicked'),
          `Expected close button ${i} to trigger onClosePanelClicked`);
    }
  });
});
