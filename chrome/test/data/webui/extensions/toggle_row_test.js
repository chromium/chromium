// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {eventToPromise} from '../test_util.js';

suite('extensions-toggle-row', function() {
  let row;

  setup(function() {
    document.body.innerHTML = `
      <extensions-toggle-row id="row">
        <span id="label">Description</span>
      </extensions-toggle-row>
    `;

    row = document.getElementById('row');
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
