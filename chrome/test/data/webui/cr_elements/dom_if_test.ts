// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';


class TestChildElement extends PolymerElement {
  static get template() {
    return html`[[someObjectValue.num]]`;
  }

  static get properties() {
    return {
      someObjectValue: Object,
    };
  }

  declare someObjectValue: {num: number};
}

customElements.define('test-child', TestChildElement);

class TestParentElement extends PolymerElement {
  static get template() {
    return html`
    <template is="dom-if" if="[[showChild]]">
      <test-child some-object-value="[[someObjectValue]]">
      </test-child>
    </template>`;
  }

  static get properties() {
    return {
      showChild: {
        type: Boolean,
        value: true,
      },

      someObjectValue: {
        type: Object,
        value: () => ({num: 0}),
      },
    };
  }

  declare showChild: boolean;
  declare someObjectValue: {num: number};
}

customElements.define('test-parent', TestParentElement);

suite('DomIf', function() {
  test('updateWhenFalse', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const element = document.createElement('test-parent') as TestParentElement;
    document.body.appendChild(element);
    await flushTasks();

    const domIfElement = element.shadowRoot!.querySelector('dom-if');
    assertTrue(!!domIfElement);
    // Ensure that the element is initially rendered.
    const childElement =
        element.shadowRoot!.querySelector<TestChildElement>('test-child');
    assertTrue(!!childElement);

    // Test initial state.
    assertFalse(domIfElement.hasAttribute('update-when-false'));
    assertEquals('0', childElement.shadowRoot!.textContent);

    element.set('someObjectValue.num', 1);
    assertEquals('1', childElement.shadowRoot!.textContent);

    // Reproduce https://github.com/Polymer/polymer/issues/4818 bug.
    element.showChild = false;
    await flushTasks();
    assertFalse(isVisible(childElement));
    element.set('someObjectValue.num', 2);
    element.showChild = true;
    await flushTasks();
    assertEquals('1', childElement.shadowRoot!.textContent);

    // Test that setting update-when-false fixes the bug.
    element.showChild = false;
    domIfElement.toggleAttribute('update-when-false', true);
    await flushTasks();
    assertFalse(isVisible(childElement));
    element.set('someObjectValue.num', 3);
    element.showChild = true;
    await flushTasks();
    assertEquals('3', childElement.shadowRoot!.textContent);
  });
});
