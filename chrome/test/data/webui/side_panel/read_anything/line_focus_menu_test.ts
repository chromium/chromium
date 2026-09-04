// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {LINE_FOCUS_FEATURE_NAME, LineFocusMovement, LineFocusStyle, ReadAnythingSettingsChange, ToolbarEvent, userEducationProxyFactory} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestUserEducationMixedTrustHandler} from 'chrome-untrusted://webui-test/test_user_education_mixed_trust_handler.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineFocusMenuElement', () => {
  let lineFocusMenu: LineFocusMenuElement;
  let metrics: TestMetricsBrowserProxy;
  let userEducationHandler: TestUserEducationMixedTrustHandler;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    const result = setupTestEnvironment();
    metrics = result.metrics;
    userEducationHandler = new TestUserEducationMixedTrustHandler();
    userEducationProxyFactory.setInstance({handler: userEducationHandler});

    lineFocusMenu = document.createElement('line-focus-menu');
    document.body.appendChild(lineFocusMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineFocusMenu);
  });

  test('has header shortcut', () => {
    assertEquals(
        loadTimeData.getString('lineFocusShortcutLabel'),
        lineFocusMenu.$.menu.menuGroups[0]!.header.shortcut);
  });

  test('notifies of feature use if enabled on close', async () => {
    lineFocusMenu.close();
    assertEquals(
        0, userEducationHandler.getCallCount('notifyNewBadgeFeatureUsed'));

    lineFocusMenu.lineFocusEnabled = true;
    await microtasksFinished();
    lineFocusMenu.close();
    assertEquals(userEducationProxyFactory, lineFocusMenu.proxy);
    assertEquals(
        1, userEducationHandler.getCallCount('notifyNewBadgeFeatureUsed'));
    assertDeepEquals(
        [LINE_FOCUS_FEATURE_NAME],
        userEducationHandler.getArgs('notifyNewBadgeFeatureUsed'));
  });

  test('line focus style prop update changes selected items', async () => {
    const window = LineFocusStyle.MEDIUM_WINDOW;
    lineFocusMenu.lineFocusStyle = window;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(window, selectedItems[0]!.data, 'data');

    const line = LineFocusStyle.UNDERLINE;
    lineFocusMenu.lineFocusStyle = line;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected line');
    assertEquals(line, selectedItems[0]!.data, 'data line');
  });

  test('line focus enabled prop update changes selected items', async () => {
    lineFocusMenu.lineFocusEnabled = true;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected true');
    assertEquals(true, selectedItems[0]!.data);

    lineFocusMenu.lineFocusEnabled = false;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected false');
    assertEquals(false, selectedItems[0]!.data);
  });

  test('on line focus style change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.LARGE_WINDOW}}));
    await microtasksFinished();

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('line focus movement prop update changes selected items', async () => {
    const cursor = LineFocusMovement.CURSOR;
    lineFocusMenu.lineFocusMovement = cursor;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuGroups[2]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(cursor, selectedItems[0]!.data);

    const staticMovement = LineFocusMovement.STATIC;
    lineFocusMenu.lineFocusMovement = staticMovement;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[2]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(staticMovement, selectedItems[0]!.data);
  });

  test('on line focus movement change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.CURSOR}}));
    await microtasksFinished();

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('on line focus toggle change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}}));
    await microtasksFinished();

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_TOGGLE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    lineFocusMenu.open(document.body);
    assertTrue(lineFocusMenu.$.menu.$.lazyMenu.get().open);
    lineFocusMenu.close();
    assertFalse(lineFocusMenu.$.menu.$.lazyMenu.get().open);
  });

  suite('with read anything improved ui', () => {
    setup(async () => {
      setupTestEnvironment({readAnythingImprovedUiEnabled: true});

      lineFocusMenu = document.createElement('line-focus-menu');
      document.body.appendChild(lineFocusMenu);
      await microtasksFinished();
    });

    test(
        'line focus enabled prop update changes visible submenus', async () => {
          assertEquals(1, lineFocusMenu.$.menu.menuGroups.length);

          lineFocusMenu.lineFocusEnabled = true;
          await microtasksFinished();

          assertEquals(3, lineFocusMenu.$.menu.menuGroups.length);
          assertEquals(
              lineFocusMenu.$.menu.menuGroups[0]!.header.title,
              loadTimeData.getString('lineFocusLabel'));
          assertEquals(
              lineFocusMenu.$.menu.menuGroups[1]!.header.title,
              loadTimeData.getString('lineFocusStyleHeading'));
          assertEquals(
              lineFocusMenu.$.menu.menuGroups[2]!.header.title,
              loadTimeData.getString('lineFocusMovementHeading'));

          lineFocusMenu.lineFocusEnabled = false;
          await microtasksFinished();

          assertEquals(1, lineFocusMenu.$.menu.menuGroups.length);
        });
  });
});
