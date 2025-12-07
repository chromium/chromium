// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectMixin} from 'chrome://print/print_preview.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SelectMixinTest', function() {
  let testSelect: TestSelectElement;

  const TestSelectElementBase = SelectMixin(PolymerElement);

  class TestSelectElement extends TestSelectElementBase {
    static get is() {
      return 'test-select';
    }

    static get template() {
      return html`
        <select value="[[selectedValue]]" on-change="onSelectChange">
          <option value="0" selected>0</option>
          <option value="1">1</option>
          <option value="2">2</option>
        </select>
      `;
    }

    selectChanges: string[] = [];

    override onProcessSelectChange(value: string) {
      this.selectChanges.push(value);
      this.dispatchEvent(new CustomEvent(
          'process-select-change-called',
          {bubbles: true, composed: true, detail: value}));
    }
  }

  customElements.define(TestSelectElement.is, TestSelectElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testSelect = document.createElement('test-select') as TestSelectElement;
    document.body.appendChild(testSelect);
    testSelect.selectedValue = '0';
  });

  // Tests that onProcessSelectChange() is called when the select value is
  // by changing the select element but not when it is set programmatically.
  test('call process select change', async () => {
    const select = testSelect.shadowRoot!.querySelector('select')!;
    assertEquals('0', testSelect.selectedValue);
    assertEquals('0', select.value);

    // Programmatically update `selectedValue`. This should update the
    // <select>'s value via the binding, but should not trigger
    // onProcessSelectChange().
    testSelect.selectedValue = '1';
    assertEquals('1', select.value);

    // Debounces by 100ms. Wait a bit longer to make sure nothing happens.
    await new Promise(resolve => setTimeout(resolve, 120));
    assertEquals(0, testSelect.selectChanges.length);

    // Change the value from the UI.
    select.value = '0';
    select.dispatchEvent(new CustomEvent('change'));
    await eventToPromise('process-select-change-called', testSelect);

    assertEquals(1, testSelect.selectChanges.length);
    assertEquals('0', testSelect.selectChanges[0]);
  });
});
