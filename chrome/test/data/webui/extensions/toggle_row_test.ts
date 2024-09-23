// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsToggleRowElement} from 'chrome://extensions/extensions.js';
import {getTrustedHTML} from 'chrome://extensions/extensions.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('extensions-toggle-row', function() {
  let row: ExtensionsToggleRowElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <extensions-toggle-row id="row">
        <span id="label">Description</span>
      </extensions-toggle-row>
    `;

    row = document.querySelector('extensions-toggle-row')!;
    assertFalse(row.checked);
  });


  // Test that the control is toggled when the user taps on the text label.
  test('TestToggleByLabelTap', function() {
    let whenChanged = eventToPromise('change', row);
    row.getLabel().click();
    return whenChanged
        .then(function() {
          assertTrue(row.checked);
          whenChanged = eventToPromise('change', row);
          row.getLabel().click();
          return whenChanged;
        })
        .then(function() {
          assertFalse(row.checked);
        });
  });
});
