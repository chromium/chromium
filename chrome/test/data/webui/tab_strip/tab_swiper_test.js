// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OPACITY_ANIMATION_THRESHOLD_PX, SWIPE_FINISH_THRESHOLD_PX, SWIPE_START_THRESHOLD_PX, TabSwiper} from 'chrome://tab-strip/tab_swiper.js';

import {eventToPromise} from '../test_util.m.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('TabSwiper', () => {
  let tabElement;
  let tabSwiper;

  const tab = {id: 1001};

  setup(() => {
    document.body.innerHTML = '';

    tabElement = document.createElement('div');
    tabElement.tab = tab;
    document.body.appendChild(tabElement);

    tabSwiper = new TabSwiper(tabElement);
    tabSwiper.startObserving();
  });

  test('swiping progresses the animation', () => {
    document.body.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);

    const startY = 50;
    const pointerState = {clientY: startY, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    function testSwipeAnimation(swipeUp) {
      const direction = swipeUp ? -1 : 1;

      // Swipe was not enough to start any part of the animation.
      pointerState.clientY = startY + (direction * 1);
      pointerState.movementY = 1; /* Any non-0 value here is fine. */
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      assertEquals(tabElStyle.maxWidth, '100px');
      assertEquals(tabElStyle.opacity, '1');

      // Swipe was enough to start animating opacity.
      pointerState.clientY =
          startY + (direction * (OPACITY_ANIMATION_THRESHOLD_PX + 1));
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      assertEquals(tabElStyle.maxWidth, '100px');
      assertTrue(
          parseFloat(tabElStyle.opacity) > 0 &&
          parseFloat(tabElStyle.opacity) < 1);

      // Swipe was enough to start animating max width.
      pointerState.clientY =
          startY + (direction * (SWIPE_START_THRESHOLD_PX + 1));
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      assertTrue(
          parseInt(tabElStyle.maxWidth) > 0 &&
          parseInt(tabElStyle.maxWidth) < 100);
      assertTrue(
          parseFloat(tabElStyle.opacity) > 0 &&
          parseFloat(tabElStyle.opacity) < 1);

      // Swipe was enough to finish animating.
      pointerState.clientY =
          startY + (direction * (SWIPE_FINISH_THRESHOLD_PX + 1));
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      assertEquals(tabElStyle.maxWidth, '0px');
      assertEquals(tabElStyle.opacity, '0');
    }

    testSwipeAnimation(true);
    testSwipeAnimation(false);
  });

  test('finishing the swipe animation fires an event', async () => {
    async function testFiredEvent(swipeUp) {
      const firedEventPromise = eventToPromise('swipe', tabElement);
      const startY = 50;
      const direction = swipeUp ? -1 : 1;
      const pointerState = {clientY: startY, pointerId: 1};
      tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

      pointerState.clientY =
          startY + (direction * (SWIPE_FINISH_THRESHOLD_PX + 1));
      pointerState.movementY = 1; /* Any non-0 value here is fine. */
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
      await firedEventPromise;
    }

    await testFiredEvent(true);

    // Re-add the element back to the DOM and re-test for swiping down.
    document.body.appendChild(tabElement);
    await testFiredEvent(false);
  });

  test('swiping enough and releasing finishes the animation', async () => {
    async function testReleasing(swipeUp) {
      const firedEventPromise = eventToPromise('swipe', tabElement);

      const tabElStyle = window.getComputedStyle(tabElement);
      const startY = 50;
      const direction = swipeUp ? -1 : 1;

      const pointerState = {clientY: 50, pointerId: 1};
      tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

      pointerState.clientY =
          startY + (direction * (SWIPE_START_THRESHOLD_PX + 1));
      pointerState.movementY = 1; /* Any non-0 value here is fine. */
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));
      await firedEventPromise;
      assertEquals(tabElStyle.maxWidth, '0px');
      assertEquals(tabElStyle.opacity, '0');
    }

    await testReleasing(true);

    // Re-add the element back to the DOM and re-test for swiping down.
    document.body.appendChild(tabElement);
    await testReleasing(false);
  });

  test('swiping and letting go before resets animation', () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');

    function testReleasing(swipeUp) {
      const tabElStyle = window.getComputedStyle(tabElement);
      const startY = 50;
      const direction = swipeUp ? -1 : 1;

      const pointerState = {clientY: 50, pointerId: 1};
      tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

      pointerState.clientY = startY + (direction * 1);
      pointerState.movementY = 1; /* Any non-0 value here is fine. */
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

      assertEquals(tabElStyle.maxWidth, '100px');
      assertEquals(tabElStyle.opacity, '1');
    }

    testReleasing(true);
    testReleasing(false);
  });

  test('swiping fast enough finishes playing the animation', async () => {
    const tabElStyle = window.getComputedStyle(tabElement);

    async function testHighSpeedSwipe(swipeUp) {
      const firedEventPromise = eventToPromise('swipe', tabElement);
      const direction = swipeUp ? -1 : 1;
      const startY = 50;
      const pointerState = {clientY: 50, pointerId: 1};

      tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

      pointerState.clientY = 100;
      pointerState.movementY = direction * 100;
      pointerState.timestamp = 1020;
      tabElement.dispatchEvent(new PointerEvent('pointermove', pointerState));
      tabElement.dispatchEvent(new PointerEvent('pointerup', pointerState));

      await firedEventPromise;
      assertEquals(tabElStyle.maxWidth, '0px');
      assertEquals(tabElStyle.opacity, '0');
    }

    await testHighSpeedSwipe(true);

    // Re-add the element back to the DOM and re-test for swiping down.
    document.body.appendChild(tabElement);
    await testHighSpeedSwipe(false);
  });

  test('pointerdown should reset the animation time', async () => {
    tabElement.style.setProperty('--tabstrip-tab-width', '100px');
    const tabElStyle = window.getComputedStyle(tabElement);
    const pointerState = {clientY: 50, pointerId: 1};
    tabElement.dispatchEvent(new PointerEvent('pointerdown', pointerState));

    // Mimic a swipe that turns into a scroll.
    pointerState.clientY += SWIPE_FINISH_THRESHOLD_PX;
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
});
