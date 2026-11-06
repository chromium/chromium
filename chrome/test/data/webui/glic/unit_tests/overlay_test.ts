// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, ErrorPanelType, GlicOverlayPageHandlerRemote, LoadingStyle} from 'chrome://glic/glic_overlay.js';
import type {GlicOverlayElement, GlicOverlayPageRemote} from 'chrome://glic/glic_overlay.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('GlicOverlayTest', () => {
  let overlay: GlicOverlayElement;
  let handler: TestMock<GlicOverlayPageHandlerRemote>&
      GlicOverlayPageHandlerRemote;
  let remote: GlicOverlayPageRemote;
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

    handler = TestMock.fromClass(GlicOverlayPageHandlerRemote);
    const testProxy = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(testProxy.instance);
    remote = testProxy.remote;

    overlay = overlayTemplate.cloneNode(true) as GlicOverlayElement;
    document.body.appendChild(overlay);
  });

  function getElement<T extends HTMLElement = HTMLElement>(id: string): T {
    const el = overlay.querySelector<T>(`#${id}`);
    assertTrue(!!el, `Element #${id} not found`);
    return el;
  }

  test('ShowLoadingFloating', async () => {
    remote.setOverlayState({loading: LoadingStyle.kFloating});
    await remote.$.flushForTesting();

    assertFalse(getElement('loadingPanel').hidden);
    assertTrue(document.body.classList.contains('floating'));
    assertFalse(document.body.classList.contains('sidePanel'));
  });

  test('ShowLoadingSidePanel', async () => {
    remote.setOverlayState({loading: LoadingStyle.kSidePanel});
    await remote.$.flushForTesting();

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
      {
        type: ErrorPanelType.kPendingProviderChangeGeminiToGeic,
        id: 'relaunchRequiredPanel',
      },
      {
        type: ErrorPanelType.kPendingProviderChangeGeicToGemini,
        id: 'relaunchRequiredPanel',
      },
      {
        type: ErrorPanelType.kPendingProviderChangeGeicToOtherGeic,
        id: 'relaunchRequiredPanel',
      },
      {type: ErrorPanelType.kLocationMismatch, id: 'locationMismatchPanel'},
    ];

    const allPanels = Array.from(
        overlay.querySelectorAll<HTMLElement>('#localPanels .dialog.panel'));

    for (const {type, id} of errorTypes) {
      remote.setOverlayState({error: type});
      await remote.$.flushForTesting();

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

  test('ShowRelaunchPanelCopyForProviderChange', async () => {
    const cases:
        Array<{type: ErrorPanelType, header: string, body: string[]}> = [
          {
            type: ErrorPanelType.kPendingProviderChangeGeminiToGeic,
            header: 'relaunchRequiredGeminiToGeicNoticeHeader',
            body: ['relaunchRequiredGeminiToGeicNotice'],
          },
          {
            type: ErrorPanelType.kPendingProviderChangeGeicToGemini,
            header: 'relaunchRequiredGeicToGeminiNoticeHeader',
            body: [
              'relaunchRequiredGeicToGeminiNotice1',
              'relaunchRequiredGeicToGeminiNotice2',
              'relaunchRequiredGeicToGeminiNotice3',
            ],
          },
          {
            type: ErrorPanelType.kPendingProviderChangeGeicToOtherGeic,
            header: 'relaunchRequiredGeicToOtherGeicNoticeHeader',
            body: ['relaunchRequiredGeicToOtherGeicNotice'],
          },
        ];

    // Run the three-paragraph case between single-paragraph cases to check
    // that unused paragraphs are hidden again.
    for (const {type, header, body} of cases) {
      remote.setOverlayState({error: type});
      await remote.$.flushForTesting();

      assertFalse(getElement('relaunchRequiredPanel').hidden);
      assertEquals(
          loadTimeData.getString(header),
          getElement('relaunchHeadline').textContent);
      for (let i = 0; i < 3; i++) {
        const paragraph = getElement(`relaunchBody${i + 1}`);
        const key = body[i];
        if (key) {
          assertFalse(paragraph.hidden);
          assertEquals(loadTimeData.getString(key), paragraph.textContent);
        } else {
          assertTrue(paragraph.hidden);
        }
      }
    }
  });

  test('ButtonClicksNotifyHandler', () => {
    const clickCases:
        Array<{id: string, method: keyof GlicOverlayPageHandlerRemote}> = [
          {id: 'retry', method: 'onRetryClicked'},
          {id: 'reload', method: 'onRetryClicked'},
          {id: 'signInButton', method: 'onSignInClicked'},
          {id: 'relaunchButton', method: 'onRelaunchClicked'},
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
          1, handler.getCallCount(method), `Expected call for ${method}`);
      handler.resetResolver(method);
    }

    const adminLink = getElement('disabledByAdminPanel').querySelector('a')!;
    adminLink.click();
    assertEquals(1, handler.getCallCount('onDisabledByAdminLinkClicked'));

    const closeButtons =
        Array.from(overlay.querySelectorAll<HTMLElement>('.close-button'));
    assertTrue(closeButtons.length > 0, 'Expected close buttons to exist');
    for (let i = 0; i < closeButtons.length; i++) {
      handler.resetResolver('onClosePanelClicked');
      closeButtons[i]!.click();
      assertEquals(
          1, handler.getCallCount('onClosePanelClicked'),
          `Expected close button ${i} to trigger onClosePanelClicked`);
    }
  });
});
