// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {LINE_FOCUS_FEATURE_NAME, LineFocusMovement, LineFocusStyle, ToolbarEvent, userEducationProxyFactory} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestUserEducationMixedTrustHandler} from 'chrome-untrusted://webui-test/test_user_education_mixed_trust_handler.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';

suite('LineFocusMenuElement', () => {
  let lineFocusMenu: LineFocusMenuElement;
  let userEducationHandler: TestUserEducationMixedTrustHandler;

  setup(() => {
    setupTestEnvironment();
    userEducationHandler = new TestUserEducationMixedTrustHandler();
    userEducationProxyFactory.setInstance({handler: userEducationHandler});

    lineFocusMenu = document.createElement('line-focus-menu');
    document.body.appendChild(lineFocusMenu);
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
    assertEquals(window, lineFocusMenu.$.menu.menuGroups[1]!.selectedValue);

    const line = LineFocusStyle.UNDERLINE;
    lineFocusMenu.lineFocusStyle = line;
    await microtasksFinished();
    assertEquals(line, lineFocusMenu.$.menu.menuGroups[1]!.selectedValue);
  });

  test('line focus enabled prop update changes selected items', async () => {
    lineFocusMenu.lineFocusEnabled = true;
    await microtasksFinished();
    assertEquals(true, lineFocusMenu.$.menu.menuGroups[0]!.selectedValue);

    lineFocusMenu.lineFocusEnabled = false;
    await microtasksFinished();
    assertEquals(false, lineFocusMenu.$.menu.menuGroups[0]!.selectedValue);
  });

  test('on line focus style change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.LARGE_WINDOW}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test('line focus movement prop update changes selected items', async () => {
    const cursor = LineFocusMovement.CURSOR;
    lineFocusMenu.lineFocusMovement = cursor;
    await microtasksFinished();
    assertEquals(cursor, lineFocusMenu.$.menu.menuGroups[2]!.selectedValue);

    const staticMovement = LineFocusMovement.STATIC;
    lineFocusMenu.lineFocusMovement = staticMovement;
    await microtasksFinished();
    assertEquals(
        staticMovement, lineFocusMenu.$.menu.menuGroups[2]!.selectedValue);
  });

  test('on line focus movement change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.CURSOR}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test('on line focus toggle change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}}));
    await microtasksFinished();

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
