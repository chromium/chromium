// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectMixin} from 'chrome://print/print_preview.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SelectMixinTest', function() {
  let testSelect: TestSelectElement;

  const TestSelectElementBase = SelectMixin(CrLitElement);

  class TestSelectElement extends TestSelectElementBase {
    static get is() {
      return 'test-select';
    }

    override render() {
      return html`
        <select .value="${this.selectedValue}" @change="${this.onSelectChange}">
          <option value="0" selected>0</option>
          <option value="1">1</option>
          <option value="2">2</option>
        </select>
      `;
    }

    selectChanges: string[] = [];

    override onProcessSelectChange(value: string) {
      this.selectChanges.push(value);
      this.fire('process-select-change-called', value);
    }
  }

  customElements.define(TestSelectElement.is, TestSelectElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testSelect = document.createElement('test-select') as TestSelectElement;
    document.body.appendChild(testSelect);
    testSelect.selectedValue = '0';
    return microtasksFinished();
  });

  // Tests that onProcessSelectChange() is called when the select value is
  // by changing the select element but not when it is set programmatically.
  test('call process select change', async () => {
    const select = testSelect.shadowRoot.querySelector('select')!;
    assertEquals('0', testSelect.selectedValue);
    assertEquals('0', select.value);

    // Programmatically update `selectedValue`. This should update the
    // <select>'s value via the binding, but should not trigger
    // onProcessSelectChange().
    testSelect.selectedValue = '1';
    await microtasksFinished();
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
