// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/categories.js';

import {CategoriesElement} from 'chrome://customize-chrome-side-panel.top-chrome/categories.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CategoriesTest', () => {
  let categoriesElement: CategoriesElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    categoriesElement = document.createElement('customize-chrome-categories');
    document.body.appendChild(categoriesElement);
  });

  test('categories buttons create events', async () => {
    // Check that clicking the back button produces a back-click event.
    let eventPromise = eventToPromise('back-click', categoriesElement);
    categoriesElement.$.backButton.click();
    let event = await eventPromise;
    assertTrue(!!event);

    // Check that clicking a category produces a category-select event.
    eventPromise = eventToPromise('category-select', categoriesElement);
    const category = categoriesElement.shadowRoot!.querySelector(
                         '.category')! as HTMLButtonElement;
    category.click();
    event = await eventPromise;
    assertTrue(!!event);
  });
});
