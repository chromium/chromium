// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
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

  setup(async () => {
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

  async function setInitialSettings(
      customLinksEnabled: boolean, shortcutsVisible: boolean): Promise<void> {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    await handler.whenCalled('updateMostVisitedSettings');
    callbackRouterRemote.setMostVisitedSettings(
        customLinksEnabled, shortcutsVisible);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(customizeShortcutsElement.$.customLinksButton.hideLabelText);
    assertTrue(customizeShortcutsElement.$.mostVisitedButton.hideLabelText);
    assertEquals(
        customizeShortcutsElement.$.customLinksButton.label, 'My shortcuts');
    assertEquals(
        customizeShortcutsElement.$.mostVisitedButton.label,
        'Most visited sites');
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
        'mostVisitedOption',
        customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ false);
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertCustomLinksEnabled();
    customizeShortcutsElement.$.mostVisitedButton.click();
    assertUseMostVisited();
    customizeShortcutsElement.$.showToggle.click();
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertUseMostVisited();
  });

  test('turning toggle on updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ false);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('turning toggle off hides settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(false, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertFalse(customLinksEnabled);
    assertFalse(shortcutsVisible);
  });

  test('clicking toggle title updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ false);

    customizeShortcutsElement.$.showToggleContainer.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('clicking custom links label updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    customizeShortcutsElement.$.customLinksContainer.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertTrue(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('clicking custom button label updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    customizeShortcutsElement.$.customLinksButton.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertTrue(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited label updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();

    customizeShortcutsElement.$.mostVisitedContainer.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited button updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();

    customizeShortcutsElement.$.mostVisitedButton.click();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('keydown on radio options updates MV settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    assertUseMostVisited();

    keyDownOn(customizeShortcutsElement.$.mostVisitedButton, 0, [], 'ArrowUp');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertTrue(customLinksEnabled);
    assertTrue(shortcutsVisible);

    keyDownOn(
        customizeShortcutsElement.$.customLinksButton, 0, [], 'ArrowDown');
    await microtasksFinished();

    assertEquals(2, handler.getCallCount('setMostVisitedSettings'));
    const [customLinksEnabled2, shortcutsVisible2] =
        handler.getArgs('setMostVisitedSettings')[1];
    assertFalse(customLinksEnabled2);
    assertTrue(shortcutsVisible2);
  });

  test('only animates after initialization', async () => {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    const crCollapse =
        customizeShortcutsElement.shadowRoot!.querySelector('cr-collapse')!;

    // No animation before initialize.
    assertTrue(crCollapse.noAnimation!);

    // Initialize.
    callbackRouterRemote.setMostVisitedSettings(
        /*customLinksEnabled=*/ true, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Animation after initialize.
    assertFalse(crCollapse.noAnimation!);

    // Update.
    callbackRouterRemote.setMostVisitedSettings(
        /*customLinksEnabled=*/ false, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Still animation after update.
    assertFalse(crCollapse.noAnimation!);
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
