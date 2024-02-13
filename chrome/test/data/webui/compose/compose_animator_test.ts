// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/animations/animator.js';

import {getTrustedHTML} from '//resources/js/static_types.js';
import {Animator} from 'chrome-untrusted://compose/animations/animator.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('ComposeAnimator', () => {
  let animator: Animator;
  let mockRoot: HTMLElement;

  function animationsAsPromises(animations: Animation[]) {
    return Promise.all(animations.map(animation => animation.finished));
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockRoot = document.createElement('custom-element');
    const shadowRoot = mockRoot.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = getTrustedHTML`
      <div class="child" id="child1"></div>
      <div class="child" id="child2"></div>
    `;
    document.body.appendChild(mockRoot);
    animator = new Animator(mockRoot, /* animationsEnabled= */ true);
  });

  test('Animates', async () => {
    const animations = animator.animate(
        '.child',
        [
          {width: '50px'},
          {width: '100px'},
        ],
        {delay: 10, duration: 50, easing: 'ease-in', fill: 'both'});
    assertEquals(2, animations.length);
    await animationsAsPromises(animations);
    animations.forEach((animation) => {
      const effect = animation.effect as KeyframeEffect;
      const timing = effect.getTiming();
      assertEquals(10, timing.delay);
      assertEquals(50, timing.duration);
      assertEquals('ease-in', timing.easing);
      assertEquals(100, (effect.target as HTMLElement).offsetWidth);
    });
  });

  test('FadesIn', async () => {
    const animations = animator.fadeIn('#child1', {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals('0', window.getComputedStyle(child).opacity);
    await animationsAsPromises(animations);
    assertEquals('1', window.getComputedStyle(child).opacity);
  });

  test('FadesOut', async () => {
    const animations = animator.fadeOut('#child1', {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals('1', window.getComputedStyle(child).opacity);
    await animationsAsPromises(animations);
    assertEquals('0', window.getComputedStyle(child).opacity);
    assertEquals('block', window.getComputedStyle(child).display);
  });

  test('FadesOutAndHides', async () => {
    const animations =
        animator.fadeOutAndHide('#child1', 'flex', {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals('1', window.getComputedStyle(child).opacity);
    assertEquals('flex', window.getComputedStyle(child).display);
    await animationsAsPromises(animations);
    assertEquals('0', window.getComputedStyle(child).opacity);
    assertEquals('none', window.getComputedStyle(child).display);
  });

  test('ScalesIn', async () => {
    const animations = animator.scaleIn('#child1', {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals(
        'matrix(0, 0, 0, 0, 0, 0)', window.getComputedStyle(child).transform);
    await animationsAsPromises(animations);
    assertEquals(
        'matrix(1, 0, 0, 1, 0, 0)', window.getComputedStyle(child).transform);
  });

  test('SlidesIn', async () => {
    const animations =
        animator.slideIn('#child1', 83, {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals(
        'matrix(1, 0, 0, 1, 0, 83)', window.getComputedStyle(child).transform);
    await animationsAsPromises(animations);
    assertEquals(
        'matrix(1, 0, 0, 1, 0, 0)', window.getComputedStyle(child).transform);
  });

  test('SlidesOut', async () => {
    const animations =
        animator.slideOut('#child1', 83, {duration: 5, fill: 'both'});
    assertEquals(1, animations.length);
    const child = mockRoot.shadowRoot!.querySelector('#child1')!;
    assertEquals(
        'matrix(1, 0, 0, 1, 0, 0)', window.getComputedStyle(child).transform);
    await animationsAsPromises(animations);
    assertEquals(
        'matrix(1, 0, 0, 1, 0, 83)', window.getComputedStyle(child).transform);
  });

  test('DisablesAnimations', () => {
    const disabledAnimator = new Animator(mockRoot, false);
    const animations = disabledAnimator.animate(
        '.child',
        [
          {width: '50px'},
          {width: '100px'},
        ],
        {delay: 10, duration: 50, easing: 'ease-in'});
    assertEquals(0, animations.length);
  });

  test('RequiresConditionMet', () => {
    const animations = animator.animate(
        '.child',
        [
          {width: '50px'},
          {width: '100px'},
        ],
        {duration: 100},
        /* falsey condition should mean no animations */ false);
    assertEquals(0, animations.length);
  });
});
