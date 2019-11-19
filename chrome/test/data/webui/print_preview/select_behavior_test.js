// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectBehavior} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.select_behavior_test = {};
select_behavior_test.suiteName = 'SelectBehaviorTest';
/** @enum {string} */
select_behavior_test.TestNames = {
  CallProcessSelectChange: 'call process select change',
};

suite(select_behavior_test.suiteName, function() {
  /** @type {?TestSelectElement} */
  let testSelect = null;

  /** @type {string} */
  let settingValue = '0';

  /** @override */
  setup(function() {
    Polymer({
      is: 'test-select',

      _template: html`
        <select value="{{selectedValue::change}}">
          <option value="0" selected>0</option>
          <option value="1">1</option>
          <option value="2">2</option>
        </select>
      `,

      behaviors: [SelectBehavior],

      onProcessSelectChange: function(value) {
        settingValue = value;
        this.fire('process-select-change-called', value);
      },
    });

    PolymerTest.clearBody();
    testSelect = document.createElement('test-select');
    document.body.appendChild(testSelect);
    testSelect.selectedValue = '0';
  });

  // Tests that onProcessSelectChange() is called when the select value is
  // set programmatically or by changing the select element.
  test(
      assert(select_behavior_test.TestNames.CallProcessSelectChange),
      function() {
        const select = testSelect.$$('select');
        assertEquals('0', testSelect.selectedValue);
        assertEquals('0', select.value);
        let whenProcessSelectCalled =
            eventToPromise('process-select-change-called', testSelect);
        testSelect.selectedValue = '1';
        // Should be debounced so settingValue has not changed yet.
        assertEquals('0', settingValue);
        return whenProcessSelectCalled
            .then((e) => {
              assertEquals('1', e.detail);
              assertEquals('1', select.value);
              whenProcessSelectCalled =
                  eventToPromise('process-select-change-called', testSelect);
              select.value = '0';
              select.dispatchEvent(new CustomEvent('change'));
              assertEquals('1', settingValue);
              return whenProcessSelectCalled;
            })
            .then((e) => {
              assertEquals('0', e.detail);
              assertEquals('0', testSelect.selectedValue);
            });
      });
});
