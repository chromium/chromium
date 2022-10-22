// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SWIPE_FINISH_THRESHOLD_PX, SWIPE_START_THRESHOLD_PX, TabSwiper, TRANSLATE_ANIMATION_THRESHOLD_PX} from 'chrome://tab-strip.top-chrome/tab_swiper.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('TabSwiper', () => {
  let tabElement: HTMLElement;
  let tabSwiper: TabSwiper;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    tabElement = document.createElement('div');
    document.body.appendChild(tabElement);

    tabSwiper = new TabSwiper(tabElement);
    tabSwiper.startObserving();
  });

  test('SwipingProgressesAnimation', () => {
    // Set margin top 0 to avoid offsetting the bounding client rect.
    document.body.style.margin = '0';

    const tabWidth = 100;
    document.body.style.setProperty('--tabstrip-tab-width', `${tabWidth}px`);

    const tabElStyle = window.getComputedStyle(tabElement);

    const startY = 50;
    const pointerState: PointerEventInit = {
      clientY: startY,
      pointerId: 1,
      pointerType: 'touch',
    };
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Swipe was not enough to start any part of the animation.
    pointerState.clientY = startY - 1;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, `${tabWidth}px`);
    assertEquals(tabElStyle.opacity, '1');
    const startTop = tabElement.getBoundingClientRect().top;
    assertEquals(startTop, 0);

    // Swipe was enough to start animating the position.
    pointerState.clientY = startY - (TRANSLATE_ANIMATION_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, `${tabWidth}px`);
    const top = tabElement.getBoundingClientRect().top;
    assertTrue(top < startTop && top > -1 * SWIPE_FINISH_THRESHOLD_PX);

    // Swipe was enough to start animating max width and opacity.
    pointerState.clientY = startY - (SWIPE_START_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertTrue(
        parseInt(tabElStyle.maxWidth, 10) > 0 &&
        parseInt(tabElStyle.maxWidth, 10) < tabWidth);
    assertTrue(
        parseFloat(tabElStyle.opacity) > 0 &&
        parseFloat(tabElStyle.opacity) < 1);

    // Swipe was enough to finish animating.
    pointerState.clientY = startY - (SWIPE_FINISH_THRESHOLD_PX + 1);
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
    assertEquals(
        tabElement.getBoundingClientRect().top, -SWIPE_FINISH_THRESHOLD_PX);
  });

  test('SwipingPastFinishThresholdFiresEvent', async () => {
    const firedEventPromise = eventToPromise('swipe', tabElement);
    const startY = 50;
    const pointerState: PointerEventInit = {
      clientY: startY,
      pointerId: 1,
      pointerType: 'touch',
    };
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - (SWIPE_FINISH_THRESHOLD_PX + 1);
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
    await firedEventPromise;
  });

  test('SwipingPastStartThresholdFinishesAnimation', async () => {
    const firedEventPromise = eventToPromise('swipe', tabElement);

    const tabElStyle = window.getComputedStyle(tabElement);
    const startY = 50;

    const pointerState:
        PointerEventInit = {clientY: 50, pointerId: 1, pointerType: 'touch'};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - (SWIPE_START_THRESHOLD_PX + 1);
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
    await firedEventPromise;
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
  });

  test('NotCompletingSwipePastThreshold', () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);
    const startY = 50;

    const pointerState:
        PointerEventInit = {clientY: 50, pointerId: 1, pointerType: 'touch'};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = startY - 1;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

    assertEquals(tabElStyle.maxWidth, '100px');
    assertEquals(tabElStyle.opacity, '1');
  });

  test('SwipingAtHighVelocityFinishesAnimation', async () => {
    const tabElStyle = window.getComputedStyle(tabElement);
    const firedEventPromise = eventToPromise('swipe', tabElement);
    const pointerState:
        PointerEventInit = {clientY: 50, pointerId: 1, pointerType: 'touch'};

    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    pointerState.clientY = -100;
    pointerState.movementY = -50;
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

    await firedEventPromise;
    assertEquals(tabElStyle.maxWidth, '0px');
    assertEquals(tabElStyle.opacity, '0');
  });

  test('PointerDownResetsAnimationTime', async () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);
    const pointerState:
        PointerEventInit = {clientY: 50, pointerId: 1, pointerType: 'touch'};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Mimic a swipe that turns into a scroll.
    pointerState.clientY! += SWIPE_FINISH_THRESHOLD_PX;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    tabElement.dispatchEvent(new PointerEvent('pointerleave', pointerState));

    // Mimic a tap.
    pointerState.clientY = 50;
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Style should reset to defaults.
    assertEquals(tabElStyle.maxWidth, '100px');
    assertEquals(tabElStyle.opacity, '1');
  });

  test('IgnoresNontouchPointers', () => {
    const pointerState:
        PointerEventInit = {clientY: 50, pointerId: 1, pointerType: 'mouse'};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));
    pointerState.clientY! += SWIPE_FINISH_THRESHOLD_PX;
    pointerState.movementY = 1; /* Any non-0 value here is fine. */
    tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
    assertFalse(tabSwiper.wasSwiping());
  });
});
