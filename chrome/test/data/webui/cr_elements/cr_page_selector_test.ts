// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';

import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('cr-page-selector', () => {
  let element: CrPageSelectorElement;

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
      <cr-page-selector attr-for-selected="path">
        <div id="a" path="a">Page A</div>
        <div id="b" path="b">Page B</div>
        <div id="c" path="c">Page C</div>
      </cr-page-selector>
    `;
    element = document.querySelector('cr-page-selector')!;
  });

  test('Only selected is visible', async() => {
    element.selected = 'a';
    await element.updateComplete;

    assertTrue(isVisible(document.body.querySelector('#a')));
    assertFalse(isVisible(document.body.querySelector('#b')));
    assertFalse(isVisible(document.body.querySelector('#c')));

    element.selected = 'c';
    await element.updateComplete;
    assertFalse(isVisible(document.body.querySelector('#a')));
    assertFalse(isVisible(document.body.querySelector('#b')));
    assertTrue(isVisible(document.body.querySelector('#c')));
  });

  test('Click does not select', async () => {
    element.selected = 'a';
    await element.updateComplete;

    const pageA = document.body.querySelector<HTMLElement>('#a');
    assertTrue(!!pageA);

    const whenClicked = new Promise<void>(resolve => {
      pageA.addEventListener('click', () => {
        element.selected = 'b';
        resolve();
      });
    });
    pageA.click();

    await whenClicked;
    await element.updateComplete;
    assertEquals('b', element.selected);
  });
});
