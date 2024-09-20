// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon/cr_iconset.js';
import '//resources/cr_elements/icons_lit.html.js';
import './cr_icon_instrumented.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-icon', function() {
  let icon: CrIconElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    icon = document.createElement('cr-icon');
    document.body.appendChild(icon);
    await microtasksFinished();
  });

  function assertSvgPath(svg: SVGElement, expectedPath: string) {
    const iconInternal = svg.querySelector('g');
    assertTrue(!!iconInternal);
    const path = iconInternal.querySelector('path');
    assertTrue(!!path);
    assertEquals(expectedPath, path.getAttribute('d'));
  }

  // Tests that cr-icons can successfully update using cr-iconset.
  test('cr-iconset', async () => {
    icon.icon = 'cr:arrow-drop-up';
    await microtasksFinished();
    let svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 14l5-5 5 5z');

    icon.icon = 'cr:arrow-drop-down';
    await microtasksFinished();
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 10l5 5 5-5z');
  });

  test('ThrowsErrorOnUnknownIconset', async () => {
    // Using a subclass of cr-icon which is instrumented to report errors in the
    // updated() lifecycle callback method, for the purposes of this test. It is
    // still exercising the relevant codepath in cr-icon itself.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('cr-icon-instrumented');
    document.body.appendChild(element);

    async function assertThrows(icon: string) {
      element.icon = icon;
      await microtasksFinished();

      try {
        await element.updatedComplete;
        assertNotReached('Should have thrown');
      } catch (e: any) {
        assertEquals(
            `Assertion failed: Could not find iconset for: '${icon}'`,
            (e as Error).message);
      }
    }

    // Check that errors are repored as expected.
    await assertThrows('does-not-exist:foo');
    await assertThrows('does-not-exist:bar');

    // Check that existing icons still work.
    element.icon = 'cr:chevron-right';
    await microtasksFinished();
    await element.updatedComplete;
  });
});
