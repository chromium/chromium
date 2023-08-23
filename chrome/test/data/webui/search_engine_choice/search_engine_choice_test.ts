// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://search-engine-choice/app.js';

import {SearchEngineChoiceAppElement} from 'chrome://search-engine-choice/app.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('SearchEngineChoiceTest', function() {
  let testElement: SearchEngineChoiceAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('search-engine-choice-app');
    document.body.appendChild(testElement);
    return waitBeforeNextRender(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('Submit button enabled on choice click', function() {
    assertTrue(testElement.$.submitButton.disabled);

    const radioButtons =
        testElement.shadowRoot!.querySelectorAll('cr-radio-button');

    assertTrue(radioButtons.length > 0);
    radioButtons[0]!.click();
    assertFalse(testElement.$.submitButton.disabled);
  });
});
