// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/searchbox/searchbox_input.js';

import type {SearchboxIconElement} from 'chrome://resources/cr_components/searchbox/searchbox_icon.js';
import type {SearchboxInputElement} from 'chrome://resources/cr_components/searchbox/searchbox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle} from './searchbox_test_utils.js';

async function createInput(properties: Partial<SearchboxInputElement> = {}):
    Promise<SearchboxInputElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const input = document.createElement('cr-searchbox-input');
  Object.assign(input, properties);
  document.body.appendChild(input);
  await microtasksFinished();
  return input;
}

suite('SearchboxInputTest', () => {
  let input: SearchboxInputElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function assertIconMaskImageUrl(element: HTMLElement, url: string) {
    const icon =
        element.shadowRoot!.querySelector<SearchboxIconElement>('#icon');
    assertTrue(!!icon);
    assertStyle(
        icon.$.icon, '-webkit-mask-image',
        `url("chrome://new-tab-page/${url}")`);
    assertStyle(icon.$.icon, 'background-image', 'none');
  }

  test('default loupe icon', async () => {
    loadTimeData.resetForTesting({
      isLensSearchbox: false,
      isTopChromeSearchbox: false,
    });
    input = await createInput(
        {searchboxIcon: 'search.svg', placeholderText: 'Search'});
    assertIconMaskImageUrl(input, 'search.svg');
  });
});
