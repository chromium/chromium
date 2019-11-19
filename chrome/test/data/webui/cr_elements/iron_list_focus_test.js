// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
// clang-format on

// test-iron-focusable is a native custom element in order to maintain
// compatibility between Polymer v2 and Polymer v3.
customElements.define('test-iron-focusable', class extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode: 'open'});
  }

  set text(value) {
    const button = this.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.textContent = value;
  }

  get text() {
    const button = this.shadowRoot.querySelector('button');
    assertTrue(!!button);
    return button.textContent;
  }

  // Pass focus to child in shadowRoot b/c iron-list expects that.
  focus() {
    const button = this.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
  }

  connectedCallback() {
    const button = document.createElement('button');
    this.shadowRoot.appendChild(button);
  }
});

suite('iron-list-focus-test', function() {
  let testDiv = null;
  let testIronList = null;

  setup(function() {
    document.body.innerHTML += `
      <div id="testDiv">
        <iron-list>
          <template>
            <test-iron-focusable text="[[item]]" tabindex$='[[tabIndex]]'>
            </test-iron-focusable>
          </template>
        </iron-list>
      </div>`;

    testDiv = document.querySelector('#testDiv');
    testIronList = document.querySelector('iron-list');

    assertTrue(!!testDiv);
    assertTrue(!!testIronList);

    testIronList.items = Array(15).fill('item').map((v, i) => `${v}${i}`);
    Polymer.dom.flush();
  });

  teardown(function() {
    testDiv.remove();
    testDiv = null;
    testIronList = null;
  });

  test('test focus is NOT preserved', function() {
    const initialFocus = testIronList.querySelector('[tabindex="0"]');
    initialFocus.focus();
    Polymer.dom.flush();
    assertEquals('item0', initialFocus.text);
    assertEquals(initialFocus, document.activeElement);

    testIronList.splice('items', 0, 1);  // Remove the item from the list.
    Polymer.dom.flush();

    const newFocus = testIronList.querySelector('[tabindex="0"]');
    assertEquals('item1', newFocus.text);
    assertNotEquals(newFocus, document.activeElement);

    newFocus.focus();
    Polymer.dom.flush();

    testIronList.splice('items', 5, 1);  // Remove a different item.
    Polymer.dom.flush();

    const sameFocus = testIronList.querySelector('[tabindex="0"]');
    assertEquals('item1', sameFocus.text);
    assertNotEquals(sameFocus, document.activeElement);
  });

  test('test focus is preserved', function() {
    testIronList.preserveFocus = true;

    const initialFocus = testIronList.querySelector('[tabindex="0"]');
    initialFocus.focus();
    Polymer.dom.flush();
    assertEquals('item0', initialFocus.text);
    assertEquals(initialFocus, document.activeElement);

    testIronList.splice('items', 0, 1);  // Remove the item from the list.
    Polymer.dom.flush();

    const newFocus = testIronList.querySelector('[tabindex="0"]');
    assertEquals('item1', newFocus.text);
    assertEquals(newFocus, document.activeElement);

    testIronList.splice('items', 5, 1);  // Remove a different item.
    Polymer.dom.flush();

    const sameFocus = testIronList.querySelector('[tabindex="0"]');
    assertEquals('item1', sameFocus.text);
    assertEquals(sameFocus, document.activeElement);
  });
});
