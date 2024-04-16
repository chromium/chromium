// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

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

  // Tests that cr-icons can successfully update using iron-iconset added to
  // the document after the icon is attached.
  test('iron-iconset added later', async () => {
    icon.icon = 'test:print';
    await microtasksFinished();
    let svg = icon.shadowRoot!.querySelector('svg');
    assertFalse(!!svg);

    // Add the iron-iconset to the document.
    const template = html`<iron-iconset-svg id="test" name="test" size="24">
      <svg>
        <defs>
          <g id="print"><path d="M19 8H5c-1.66 0-3 1.34-3 3v6h4v4h12v-4h4v-6c0-1.66-1.34-3-3-3zm-3 11H8v-5h8v5zm3-7c-.55 0-1-.45-1-1s.45-1 1-1 1 .45 1 1-.45 1-1 1zm-1-9H6v4h12V3z"></path></g>
        </defs>
      </svg>
    </iron-iconset-svg>
    `;
    document.head.appendChild(template.content);
    await microtasksFinished();
    svg = icon.shadowRoot!.querySelector('svg');
    assertTrue(!!svg);
  });
});
