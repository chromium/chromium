// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

// clang-format on

suite('cr-lazy-render', function() {
  /** @type {!CrLazyRenderElement} */
  let lazy;

  let bind;

  suiteSetup(function() {
  });

  setup(function() {
    const template = `
        <dom-bind>
          <template>
            <cr-lazy-render id="lazy">
              <template>
                <h1>
                  <cr-checkbox checked="{{checked}}"></cr-checkbox>
                  {{name}}
                </h1>
              </template>
            </cr-lazy-render>
          </template>
        </dom-bind>`;
    document.body.innerHTML = template;
    lazy =
        /** @type {!CrLazyRenderElement} */ (document.getElementById('lazy'));
    bind = document.querySelector('dom-bind');
  });

  test('stamps after get()', function() {
    assertFalse(!!document.body.querySelector('h1'));
    assertFalse(!!lazy.getIfExists());

    const inner = lazy.get();
    assertEquals('H1', inner.nodeName);
    assertEquals(inner, document.body.querySelector('h1'));
  });

  test('one-way binding works', function() {
    bind.name = 'Wings';

    const inner = lazy.get();
    assertNotEquals(-1, inner.textContent.indexOf('Wings'));
    bind.name = 'DC';
    assertNotEquals(-1, inner.textContent.indexOf('DC'));
  });

  test('two-way binding works', function() {
    bind.checked = true;

    const inner = lazy.get();
    const checkbox = document.querySelector('cr-checkbox');
    assertTrue(checkbox.checked);
    checkbox.click();
    assertFalse(checkbox.checked);
    assertFalse(bind.checked);
  });
});
