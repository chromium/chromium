// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent, VisualBrowserProxyImpl} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('LineSpacing', () => {
  let lineSpacingMenu: LineSpacingMenuElement;
  let metrics: TestMetricsBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    visualBrowserProxy = new TestVisualBrowserProxy();
    VisualBrowserProxyImpl.setInstance(visualBrowserProxy);
    metrics = mockMetrics();

    lineSpacingMenu = document.createElement('line-spacing-menu');
    document.body.appendChild(lineSpacingMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineSpacingMenu);
  });



  test('spacing change', async () => {
    const veryLoose = visualBrowserProxy.getVeryLooseLineSpacing();
    const numberOfItems = 3;

    const closePromise1 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    lineSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_SPACING, {detail: {data: veryLoose}}));
    await closePromise1;
    assertEquals(
        veryLoose, await visualBrowserProxy.whenCalled('onLineSpacingChange'));

    visualBrowserProxy.resetResolver('onLineSpacingChange');
    const loose = visualBrowserProxy.getLooseLineSpacing();
    const closePromise2 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: loose}}));
    await closePromise2;
    assertEquals(
        loose, await visualBrowserProxy.whenCalled('onLineSpacingChange'));

    visualBrowserProxy.resetResolver('onLineSpacingChange');
    const standard = visualBrowserProxy.getStandardLineSpacing();
    const closePromise3 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: standard}}));
    await closePromise3;
    assertEquals(
        standard, await visualBrowserProxy.whenCalled('onLineSpacingChange'));

    assertEquals(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfItems, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved spacing option', async () => {
    const spacing = visualBrowserProxy.getVeryLooseLineSpacing();
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    lineSpacingMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      lineSpacing: spacing,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, lineSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;

    lineSpacingMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      lineSpacing: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, lineSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    lineSpacingMenu.open(document.body);
    assertTrue(lineSpacingMenu.$.menu.$.lazyMenu.get().open);
    lineSpacingMenu.close();
    assertFalse(lineSpacingMenu.$.menu.$.lazyMenu.get().open);
  });
});
