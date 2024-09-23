// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';

import type {CrIconsetElement} from 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-iconset', function() {
  let iconset: CrIconsetElement;

  // Super simple test element to render icons into.
  class TestElement extends CrLitElement {
    static get is() {
      return 'test-element';
    }
  }

  customElements.define(TestElement.is, TestElement);

  suiteSetup(function() {
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
    iconset = document.head.querySelector('cr-iconset')!;
    await microtasksFinished();
  });

  function assertSvgPath(svg: SVGElement, expectedPath: string) {
    const iconInternal = svg.querySelector('g');
    assertTrue(!!iconInternal);
    const path = iconInternal.querySelector('path');
    assertTrue(!!path);
    assertEquals(expectedPath, path.getAttribute('d'));
  }

  function assertSvgStyle(svg: SVGElement) {
    assertEquals('100%', svg.style.height);
    assertEquals('100%', svg.style.width);
    assertEquals('block', svg.style.display);
  }

  test('initial state', () => {
    assertEquals(iconset, IconsetMap.getInstance().get('cr-test'));
    assertEquals('none', window.getComputedStyle(iconset).display);
    const icons = iconset.querySelectorAll('g[id]');
    assertEquals(2, icons.length);
    assertEquals('arrow-drop-up', icons[0]!.id);
    assertEquals('arrow-drop-down', icons[1]!.id);
  });

  test('creates icons', () => {
    const arrowUpIcon = iconset.createIcon('arrow-drop-up');
    assertTrue(!!arrowUpIcon);
    assertSvgPath(arrowUpIcon, 'M7 14l5-5 5 5z');
    assertSvgStyle(arrowUpIcon);

    const arrowDownIcon = iconset.createIcon('arrow-drop-down');
    assertTrue(!!arrowDownIcon);
    assertSvgPath(arrowDownIcon, 'M7 10l5 5 5-5z');
    assertSvgStyle(arrowDownIcon);

    // Should return null for an icon not in the iconset
    const invalidIcon = iconset.createIcon('not-in-iconset');
    assertEquals(null, invalidIcon);
  });

  test('icon add/remove', () => {
    const icon = document.createElement('test-element');
    document.body.appendChild(icon);

    iconset.applyIcon(icon, 'arrow-drop-up');
    let svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 14l5-5 5 5z');

    // Applying a new icon removes the old one.
    iconset.applyIcon(icon, 'arrow-drop-down');
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 10l5 5 5-5z');

    // Removing the icon works.
    iconset.removeIcon(icon);
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(0, svgs.length);
  });
});
