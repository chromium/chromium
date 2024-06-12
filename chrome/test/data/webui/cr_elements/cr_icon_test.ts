// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon/cr_iconset.js';
import '//resources/cr_elements/icons.html.js';
import './cr_icon_instrumented.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import {getTrustedHTML} from '//resources/js/static_types.js';
import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-icon', function() {
  let icon: CrIconElement;

  suiteSetup(function() {
    // Add a test cr-iconset to the page. Necessary since there are not yet
    // any cr-iconsets in prod that can be imported instead.
    const div = document.createElement('div');
    div.innerHTML = getTrustedHTML`
      <cr-iconset name="cr-test" size="24">
        <svg>
          <defs>
            <g id="arrow-drop-up">
              <path d="M7 14l5-5 5 5z"></path>
            </g>
            <g id="arrow-drop-down">
              <path d="M7 10l5 5 5-5z"></path>
            </g>
          </defs>
        </svg>
      </cr-iconset>`;
    document.head.appendChild(div.querySelector('cr-iconset')!);
  });

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

  // Tests that cr-icons can successfully update using iron-iconset.
  test('iron-iconset', async () => {
    icon.icon = 'cr:chevron-left';
    await microtasksFinished();
    let svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M15.41 7.41L14 6l-6 6 6 6 1.41-1.41L10.83 12z');

    icon.icon = 'cr:chevron-right';
    await microtasksFinished();
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z');
  });

  test('cr-iconset', async () => {
    icon.icon = 'cr-test:arrow-drop-up';
    await microtasksFinished();
    let svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 14l5-5 5 5z');

    icon.icon = 'cr-test:arrow-drop-down';
    await microtasksFinished();
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 10l5 5 5-5z');
  });

  test('cr-iconset used rather than iron-iconset', async () => {
    // Add an iron-iconset to the document.
    const template = html`<iron-iconset-svg name="cr20-test" size="20">
      <svg>
        <defs>
          <g id="arrow">
            <path d="M7 10l5 5 5-5z"></path>
          </g>
        </defs>
      </svg>
    </iron-iconset-svg>`;
    document.head.appendChild(template.content);

    // Add a cr-iconset with the same name.
    const div = document.createElement('div');
    div.innerHTML = getTrustedHTML`
      <cr-iconset name="cr20-test" size="20">
        <svg>
          <defs>
            <g id="arrow">
              <path d="M7 14l5-5 5 5z"></path>
            </g>
          </defs>
        </svg>
      </cr-iconset>`;
    document.head.appendChild(div.querySelector('cr-iconset')!);

    icon.icon = 'cr20-test:arrow';
    await microtasksFinished();
    const svg = icon.shadowRoot!.querySelector('svg');
    assertTrue(!!svg);

    // Confirm the cr-iconset value.
    assertSvgPath(svg, 'M7 14l5-5 5 5z');
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
