// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

class TestElement extends CustomElement {
  static override get template() {
    return getTrustedHTML`<div id="content"></div>`;
  }
}

customElements.define('test-element', TestElement);

let testElement: TestElement;

suite('CustomElementTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  test('Template', function() {
    assertEquals(
        TestElement.template.toString(), testElement.shadowRoot!.innerHTML);
  });

  test('Test $()', function() {
    assertTrue(
        testElement.$('#content') ===
        testElement.shadowRoot!.getElementById('content'));
  });

  test('Test $all()', function() {
    assertTrue(
        testElement.$all('#content')[0] ===
        testElement.shadowRoot!.getElementById('content'));
  });

  test('Test getRequiredElement()', function() {
    assertTrue(
        testElement.getRequiredElement('#content') ===
        testElement.shadowRoot!.getElementById('content'));
  });
});
