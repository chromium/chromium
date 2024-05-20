// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {SanitizeDoneElement} from 'chrome://sanitize/sanitize_done.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

function initSanitizeDoneElement(): SanitizeDoneElement {
  const element = new SanitizeDoneElement();
  document.body.appendChild(element);
  flushTasks();
  return element;
}
suite('SanitizeUITest', function() {
  test('SanitizeDonePopulation', () => {
    const doneElement = initSanitizeDoneElement();
    const headerDiv = doneElement.shadowRoot!.querySelector('#header');
    // Verify the header element exists
    assert(headerDiv);
    // Check the header content
    assertEquals('Sanitize Done', headerDiv!.textContent);
  });
});
