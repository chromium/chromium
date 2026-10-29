// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/search/animated_glow.js';

import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {GlowAnimationState, GlowMode} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SearchAnimatedGlowElementTest', () => {
  let element: SearchAnimatedGlowElement;

  suiteSetup(() => {
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {composeboxDragAndDropHint: 'Drop files here'};
    } else {
      loadTimeData.overrideValues({
        composeboxDragAndDropHint: 'Drop files here',
      });
    }
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('search-animated-glow');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('defaults to outer-glow mode', () => {
    assertEquals(GlowMode.OUTER_GLOW, element.glowMode);
    assertEquals('outer-glow', element.getAttribute('glow-mode'));
  });

  test('renders energy effect wrappers in outer-glow mode', async () => {
    element.glowMode = GlowMode.OUTER_GLOW;
    element.animationState = GlowAnimationState.EXPANDING;
    await microtasksFinished();

    assertTrue(!!element.shadowRoot.querySelector('.gradient-blur-wrapper'));
    assertTrue(!!element.shadowRoot.querySelector('.gradient-sharp-wrapper'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-outer-glow'));
  });

  test('renders legacy inner-glow DOM in inner-glow mode', async () => {
    element.glowMode = GlowMode.INNER_GLOW;
    element.animationState = GlowAnimationState.EXPANDING;
    await microtasksFinished();

    assertEquals('inner-glow', element.getAttribute('glow-mode'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-blur-wrapper'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-sharp-wrapper'));
    assertTrue(!!element.shadowRoot.querySelector('.gradient-outer-glow'));
  });

  test('does not render gradient elements in none mode', async () => {
    element.glowMode = GlowMode.NONE;
    element.animationState = GlowAnimationState.EXPANDING;
    await microtasksFinished();

    assertEquals('none', element.getAttribute('glow-mode'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-blur-wrapper'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-sharp-wrapper'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient-outer-glow'));
    assertFalse(!!element.shadowRoot.querySelector('.gradient'));
    assertFalse(!!element.shadowRoot.querySelector('.double-gradient'));
    assertFalse(!!element.shadowRoot.querySelector('.double-gradient-mask'));
    assertTrue(!!element.shadowRoot.querySelector('.background'));
    assertTrue(!!element.shadowRoot.querySelector('#dragDropPlaceholder'));
  });

  test(
      'dragging angle updates only run in outer-glow energy effect mode',
      async () => {
        element.glowMode = GlowMode.OUTER_GLOW;
        element.animationState = GlowAnimationState.DRAGGING;
        await microtasksFinished();
        await new Promise(resolve => requestAnimationFrame(resolve));

        assertTrue(element.style.getPropertyValue('--mask-angle') !== '');
        assertTrue(element.style.getPropertyValue('--gradient-angle') !== '');

        element.animationState = GlowAnimationState.NONE;
        await microtasksFinished();

        assertEquals('', element.style.getPropertyValue('--mask-angle'));
        assertEquals('', element.style.getPropertyValue('--gradient-angle'));
      });
});
