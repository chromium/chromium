// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {RateMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, getItemsInMenu, setupTestEnvironment} from './common.js';

suite('RateMenuElement', () => {
  let rateMenu: RateMenuElement;

  setup(() => {
    setupTestEnvironment();

    rateMenu = document.createElement('rate-menu');
    document.body.appendChild(rateMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(rateMenu);
  });

  test('selecting a rate fires rate event with the rate', async () => {
    const ratePromise = eventToPromise<CustomEvent<{data: number}>>(
        ToolbarEvent.RATE, document);
    // Options are 0.5, 0.8, 1, ...
    getItemsInMenu(rateMenu.$.menu.$.lazyMenu)[1]!.click();
    const event = await ratePromise;
    assertEquals(0.8, event.detail.data);
  });

  test('restores saved rate option', async () => {
    const rate = 1.2;
    const startingIndex = rateMenu.$.menu.currentSelectedIndex;
    assertNotEquals(rate, startingIndex);

    rateMenu.speechRate = rate;
    await microtasksFinished();

    assertNotEquals(startingIndex, rateMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved rate is the same', async () => {
    const startingIndex = rateMenu.$.menu.currentSelectedIndex;

    rateMenu.speechRate = 1;
    await microtasksFinished();

    assertEquals(startingIndex, rateMenu.$.menu.currentSelectedIndex);
  });

  // <if expr="is_chromeos">
  test('ChromeOS number of rate options correct', () => {
    // Should include 0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4
    const expectedRateOptions = 8;
    const rateOptions =
        rateMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.dropdown-item');
    assertEquals(expectedRateOptions, rateOptions.length);
  });
  // </if>

  // <if expr="not is_chromeos">
  test('Non-ChromeOS number of rate options correct', () => {
    // Should include 0.5, 0.8, 1, 1.2, 1.5, 2.
    const expectedRateOptions = 6;
    const rateOptions =
        rateMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.dropdown-item');
    assertEquals(expectedRateOptions, rateOptions.length);
  });
  // </if>
});
