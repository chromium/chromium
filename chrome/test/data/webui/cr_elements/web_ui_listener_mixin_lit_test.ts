// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

const TestElementBase = WebUiListenerMixinLit(CrLitElement);
class TestElement extends TestElementBase {}
customElements.define('test-element', TestElement);

suite('WebUiListenerMixinTest', function() {
  let testElement: TestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  test('addRemoveListener', function() {
    const eventName = 'dummyEvent';
    let counter = 0;

    testElement.addWebUiListener(eventName, () => counter++);
    webUIListenerCallback(eventName);
    assertEquals(1, counter);

    webUIListenerCallback(eventName);
    assertEquals(2, counter);

    testElement.remove();
    webUIListenerCallback(eventName);
    assertEquals(2, counter);
  });
});
