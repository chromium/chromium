// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent, VisualBrowserProxyImpl} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LetterSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('LetterSpacing', () => {
  let letterSpacingMenu: LetterSpacingMenuElement;
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

    letterSpacingMenu = document.createElement('letter-spacing-menu');
    document.body.appendChild(letterSpacingMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(letterSpacingMenu);
  });



  test('spacing change', async () => {
    const numberOfSpacings = 3;

    const veryWide = visualBrowserProxy.getVeryWideLetterSpacing();
    const closePromise1 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: veryWide}}));
    await closePromise1;
    assertEquals(
        veryWide, await visualBrowserProxy.whenCalled('onLetterSpacingChange'));

    visualBrowserProxy.resetResolver('onLetterSpacingChange');
    const wide = visualBrowserProxy.getWideLetterSpacing();
    const closePromise2 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LETTER_SPACING, {detail: {data: wide}}));
    await closePromise2;
    assertEquals(
        wide, await visualBrowserProxy.whenCalled('onLetterSpacingChange'));

    visualBrowserProxy.resetResolver('onLetterSpacingChange');
    const standard = visualBrowserProxy.getStandardLetterSpacing();
    const closePromise3 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: standard}}));
    await closePromise3;
    assertEquals(
        standard, await visualBrowserProxy.whenCalled('onLetterSpacingChange'));

    assertEquals(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfSpacings, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved spacing option', async () => {
    const spacing = visualBrowserProxy.getVeryWideLetterSpacing();
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    letterSpacingMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      letterSpacing: spacing,
    };
    await microtasksFinished();

    assertNotEquals(
        startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;

    letterSpacingMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      letterSpacing: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    letterSpacingMenu.open(document.body);
    assertTrue(letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    letterSpacingMenu.close();
    assertFalse(letterSpacingMenu.$.menu.$.lazyMenu.get().open);
  });
});
