// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement} from 'chrome://compose/app.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ComposeApp', () => {
  let app: ComposeAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('compose-app');
    document.body.appendChild(app);
    return flushTasks();
  });

  test('SubmitsInput', () => {
    // Starts off with submit disabled since input is empty.
    assertTrue(isVisible(app.$.submitButton));
    assertTrue(app.$.submitButton.disabled);
    assertFalse(isVisible(app.$.resultContainer));
    assertFalse(isVisible(app.$.insertButton));

    // Inputting text enables submit.
    app.$.textarea.value = 'Here is my input.';
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
    assertFalse(app.$.submitButton.disabled);

    // Clicking on submit gets results.
    app.$.submitButton.click();
    assertFalse(isVisible(app.$.submitButton));
    assertTrue(app.$.textarea.readonly);
    assertTrue(isVisible(app.$.resultContainer));
    assertTrue(isVisible(app.$.insertButton));
  });
});
