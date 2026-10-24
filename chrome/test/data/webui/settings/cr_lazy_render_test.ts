// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {CrLazyRenderElement} from 'chrome://settings/lazy_load.js';
import {getTrustedHTML} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('cr-lazy-render', function() {
  let lazy: CrLazyRenderElement<HTMLElement>;

  interface BindData {
    name: string;
    checked: boolean;
  }

  let bind: HTMLElement&BindData;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
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
    lazy = document.body.querySelector('cr-lazy-render')!;
    bind = document.body.querySelector<HTMLElement&BindData>('dom-bind')!;
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

  test('two-way binding works', async function() {
    bind.checked = true;

    lazy.get();
    const checkbox = document.querySelector('cr-checkbox')!;
    assertTrue(checkbox.checked);
    checkbox.click();
    await checkbox.updateComplete;
    assertFalse(checkbox.checked);
    assertFalse(bind.checked);
  });
});
