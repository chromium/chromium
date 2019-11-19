// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.number_settings_section_test = {};
number_settings_section_test.suiteName = 'NumberSettingsSectionTest';
/** @enum {string} */
number_settings_section_test.TestNames = {
  BlocksInvalidKeys: 'blocks invalid keys',
};

suite(number_settings_section_test.suiteName, function() {
  let numberSettings = null;
  let parentElement = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    document.body.innerHTML = `
        <div id="parentElement">
          <print-preview-number-settings-section id="numberSettings"
              min-value="1" max-value="100" default-value="50"
              hint-message="incorrect value entered" input-valid>
          </print-preview-number-settings-section>
        </div>`;
    parentElement = document.querySelector('#parentElement');
    numberSettings = document.querySelector('#numberSettings');
  });

  // Test that key events that would result in invalid values are blocked.
  test(
      assert(number_settings_section_test.TestNames.BlocksInvalidKeys),
      function() {
        const input = numberSettings.$.userValue;
        /**
         * @param {number} code Code for the keyboard event that will be fired.
         * @param {string} key Key name for the keyboard event that will be
         *     fired.
         * @return {!Promise<!KeyboardEvent>} Promise that resolves when
         *     'keydown' is received by |parentElement|.
         */
        const sendKeyDownAndReturnPromise = (code, key) => {
          const whenKeyDown = eventToPromise('keydown', parentElement);
          keyEventOn(input.inputElement, 'keydown', code, undefined, key);
          return whenKeyDown;
        };

        return sendKeyDownAndReturnPromise(69, 'e')
            .then(e => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise(110, '.');
            })
            .then(e => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise(109, '-');
            })
            .then(e => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise(69, 'E');
            })
            .then(e => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise(187, '+');
            })
            .then(e => {
              assertTrue(e.defaultPrevented);
              // Try a valid key.
              return sendKeyDownAndReturnPromise(49, '1');
            })
            .then(e => {
              assertFalse(e.defaultPrevented);
            });
      });
});
