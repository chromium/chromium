// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
import type {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('ShortcutsTest', () => {
  let customizeShortcutsElement: ShortcutsElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
  });

  function getCustomLinksButton(): CrRadioButtonElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '[name="customLinksOption"]')!;
  }

  function getTopSitesButton(): CrRadioButtonElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '[name="topSitesOption"]')!;
  }

  function getCustomLinksContainer(): HTMLElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#customLinksContainer')!;
  }

  function getTopSitesContainer(): HTMLElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#topSitesContainer')!;
  }

  async function setInitialSettings(
      shortcutsType: number, shortcutsVisible: boolean): Promise<void> {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    await handler.whenCalled('updateMostVisitedSettings');
    callbackRouterRemote.setMostVisitedSettings(
        shortcutsType, shortcutsVisible);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(getCustomLinksButton().hideLabelText);
    assertTrue(getTopSitesButton().hideLabelText);
    assertEquals(getCustomLinksButton().label, 'My shortcuts');
    assertEquals(getTopSitesButton().label, 'Most visited sites');
  }

  function assertShown(shown: boolean) {
    assertEquals(shown, customizeShortcutsElement.$.showToggle.checked);
  }

  function assertCustomLinksEnabled() {
    assertEquals(
        'customLinksOption',
        customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  function assertUseMostVisited() {
    assertEquals(
        'topSitesOption', customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  function assertNothingSelected() {
    assertEquals(
        undefined, customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 1, /* shortcutsVisible= */ false);
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertCustomLinksEnabled();
    getTopSitesButton().click();
    assertUseMostVisited();
    customizeShortcutsElement.$.showToggle.click();
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertUseMostVisited();
  });

  test('turning toggle on updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ false);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(0, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('turning toggle off hides settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ true);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(false, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(0, shortcutsType);
    assertFalse(shortcutsVisible);
  });

  test('clicking toggle title updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ false);

    customizeShortcutsElement.$.showToggleContainer.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(0, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('clicking custom links label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    getCustomLinksContainer().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(1, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('clicking custom button label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    getCustomLinksButton().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(1, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 1, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();

    getTopSitesContainer().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(0, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited button updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 1, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();

    getTopSitesButton().click();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(0, shortcutsType);
    assertTrue(shortcutsVisible);
  });

  test('keydown on radio options updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 0, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    keyDownOn(getTopSitesButton(), 0, [], 'ArrowUp');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(1, shortcutsType);
    assertTrue(shortcutsVisible);

    keyDownOn(getCustomLinksButton(), 0, [], 'ArrowDown');
    await microtasksFinished();

    assertEquals(2, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsType2, shortcutsVisible2] =
        handler.getArgs('setMostVisitedSettings')[1];
    assertEquals(0, shortcutsType2);
    assertTrue(shortcutsVisible2);
  });

  test('no radio option selected if shortcuts type is invalid', async () => {
    await setInitialSettings(
        /* shortcutsType= */ 100, /* shortcutsVisible= */ true);
    assertNothingSelected();
  });

  test('only animates after initialization', async () => {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    const crCollapse =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse')!;

    // No animation before initialize.
    assertTrue(crCollapse.noAnimation);

    // Initialize.
    callbackRouterRemote.setMostVisitedSettings(
        /*shortcutsType=*/ 1, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Animation after initialize.
    assertFalse(crCollapse.noAnimation);

    // Update.
    callbackRouterRemote.setMostVisitedSettings(
        /*shortcutsType=*/ 0, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Still animation after update.
    assertFalse(crCollapse.noAnimation);
  });

  suite('Metrics', () => {
    test('Clicking show shortcuts toggle sets metric', async () => {
      customizeShortcutsElement.$.showToggle.click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED));

      customizeShortcutsElement.$.showToggleContainer.click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      assertEquals(
          2, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          2,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED));
    });
  });
});
