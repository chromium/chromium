// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StrictQueryMixin} from 'chrome://resources/ash/common/typescript_utils/strict_query_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertThrows} from 'chrome://webui-test/chai_assert.js';

const TestElementBase = StrictQueryMixin(PolymerElement);
class TestElement extends TestElementBase {
  static get template() {
    return html`
      <div id='container'>
        <h1 id='title'>This is the title</h1>
        <p id='p1'>Some paragraph text</p>
        <div>A div</div>
        <ul id='bulletList'>
          <li id='list-item'>List item 1</li>
          <li>List item 2</li>
        </ul>
        <span>Span text</span>
      </div>
    `;
  }
}
customElements.define('test-element', TestElement);

suite('StrictQueryMixinTest', function() {
  let testElement: TestElement;

  setup(function() {
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  test('BasicQuery', function() {
    const queriedElement = testElement.strictQuery('span', HTMLSpanElement);
    assertEquals('<span>Span text</span>', queriedElement.outerHTML);
  });

  test('QueryWithMultipleMatches', function() {
    const queriedElement = testElement.strictQuery('li', HTMLLIElement);
    // We expect the first ` li` to be returned.
    assertEquals(
        '<li id="list-item">List item 1</li>', queriedElement.outerHTML);
  });

  test('StrictQueryDiv', function() {
    const queriedElement = testElement.strictQueryDiv('#container > div');
    assertEquals('<div>A div</div>', queriedElement.outerHTML);
  });

  test('StrictQuerySpan', function() {
    const queriedElement = testElement.strictQuerySpan('span');
    assertEquals('<span>Span text</span>', queriedElement.outerHTML);
  });

  test('ThrowsOnNoMatch', function() {
    assertThrows(() => testElement.strictQueryDiv('#does-not-exist'));
  });

  test('ThrowsOnInvalidType', function() {
    assertThrows(() => testElement.strictQuery('div', HTMLSpanElement));
  });
});
