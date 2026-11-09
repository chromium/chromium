// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PlaceholderTextCycler} from 'chrome://resources/cr_components/searchbox/placeholder_text_cycler.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {waitForAttributeChange} from './searchbox_test_utils.js';

suite('PlaceholderTextCyclerTest', () => {
  let testInputElement: HTMLInputElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testInputElement = document.createElement('input');
    testInputElement.type = 'text';
    document.body.appendChild(testInputElement);
  });

  test('start and stop cycling input placeholder', async () => {
    const sampleTransitionPlaceholder = 'Make a plan';
    const placeholderTextCycler: PlaceholderTextCycler =
        new PlaceholderTextCycler(
            testInputElement, ['Ask Google', sampleTransitionPlaceholder], 50,
            25);
    placeholderTextCycler.start();
    const text =
        await waitForAttributeChange(testInputElement, 'placeholder', '');
    assertEquals(sampleTransitionPlaceholder, text);

    placeholderTextCycler.stop();
  });

  test(
      'notifies onPlaceholderChange and animates custom animationTarget',
      async () => {
        const customTarget = document.createElement('div');
        document.body.appendChild(customTarget);
        const observedPlaceholders: string[] = [];

        const cycler = new PlaceholderTextCycler(
            testInputElement, ['First hint', 'Second hint'], 50, 25, {
              animationTarget: customTarget,
              onPlaceholderChange: (placeholder: string) => {
                observedPlaceholders.push(placeholder);
              },
            });

        cycler.start();
        assertEquals('First hint', testInputElement.placeholder);
        assertEquals(1, observedPlaceholders.length);
        assertEquals('First hint', observedPlaceholders[0]);
        assertEquals(1, customTarget.getAnimations().length);
        assertEquals(0, testInputElement.getAnimations().length);

        // Advance deterministically through HOLD -> FADE_OUT -> FADE_IN.
        for (let i = 0; i < 3; i++) {
          const anim = customTarget.getAnimations()[0];
          assertTrue(!!anim);
          const finished = eventToPromise('finish', anim);
          anim.finish();
          await finished;
        }
        assertEquals('Second hint', testInputElement.placeholder);
        assertEquals('Second hint', observedPlaceholders.at(-1));

        cycler.stop();
        assertEquals('First hint', testInputElement.placeholder);
        assertEquals('First hint', observedPlaceholders.at(-1));
        assertEquals(0, customTarget.getAnimations().length);
      });
});
