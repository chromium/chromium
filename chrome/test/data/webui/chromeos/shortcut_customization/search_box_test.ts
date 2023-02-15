// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_box.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SearchBoxElement} from 'chrome://shortcut-customization/js/search/search_box.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

function initSearchBoxElement(): SearchBoxElement {
  const element = document.createElement('search-box');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('searchBoxTest', function() {
  let searchBoxElement: SearchBoxElement|null = null;

  teardown(() => {
    if (searchBoxElement) {
      searchBoxElement.remove();
    }
    searchBoxElement = null;
  });

  test('SearchBoxLoaded', async () => {
    searchBoxElement = initSearchBoxElement();
    await flush();
    assertTrue(!!searchBoxElement);
  });
});
