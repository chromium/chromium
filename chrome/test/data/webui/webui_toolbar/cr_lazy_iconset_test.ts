// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {CrLazyIconset, getTrustedHTML, IconsetMap} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('CrLazyIconsetTest', function() {
  let lazyIconset: CrLazyIconset;

  class TestDummyElement extends HTMLElement {
    constructor() {
      super();
      this.attachShadow({mode: 'open'});
    }
  }

  customElements.define('test-dummy', TestDummyElement);

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    IconsetMap.resetInstanceForTesting(new IconsetMap());
    lazyIconset = new CrLazyIconset('cr-lazy-test');
    lazyIconset.registerIcons({
      'arrow-drop-up': () =>
          getTrustedHTML`<svg><g id="arrow-drop-up"><path d="M7 14l5-5 5 5z"></path></g></svg>`,
      'arrow-drop-down': () =>
          getTrustedHTML`<svg><g id="arrow-drop-down"><path d="M7 10l5 5 5-5z"></path></g></svg>`,
    });
  });

  function assertSvgPath(svg: SVGElement, expectedPath: string) {
    assertTrue(!!svg.querySelector(`g path[d='${expectedPath}']`));
  }

  test('applyIcon, createIcon, removeIcon', () => {
    const icon = document.createElement('test-dummy') as TestDummyElement;
    document.body.appendChild(icon);

    lazyIconset.applyIcon(icon, 'arrow-drop-up');
    let svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 14l5-5 5 5z');

    // Applying a new icon removes the old one.
    lazyIconset.applyIcon(icon, 'arrow-drop-down');
    svgs = icon.shadowRoot!.querySelectorAll('svg');
    assertEquals(1, svgs.length);
    assertSvgPath(svgs[0]!, 'M7 10l5 5 5-5z');

    // Removing the icon works.
    lazyIconset.removeIcon(icon);
    assertFalse(!!icon.shadowRoot!.querySelector('svg'));

    // createIcon creates a standalone SVG element.
    const createdSvg = lazyIconset.createIcon('arrow-drop-up');
    assertTrue(!!createdSvg);
    assertSvgPath(createdSvg, 'M7 14l5-5 5 5z');
  });

  test('lazy icon registration and parsing', () => {
    let callCount = 0;
    lazyIconset.registerIcons({
      'lazy-icon': () => {
        callCount++;
        return getTrustedHTML`<svg><g id="lazy-icon"><path d="M0 0h24v24H0z"></path></g></svg>`;
      },
    });

    // Verify it is not evaluated upon registration.
    assertEquals(0, callCount);

    // Verify it is evaluated when requested.
    const icon = lazyIconset.createIcon('lazy-icon');
    assertTrue(!!icon);
    assertEquals(1, callCount);
    assertSvgPath(icon, 'M0 0h24v24H0z');

    // Verify it is cached and not evaluated again on subsequent requests.
    const icon2 = lazyIconset.createIcon('lazy-icon');
    assertTrue(!!icon2);
    assertEquals(1, callCount);
  });
});
