// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableBehavior} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {flush, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

// clang-format on

suite('cr-scrollable-behavior', function() {
  const TestElementBase =
      mixinBehaviors([CrScrollableBehavior], PolymerElement) as
      {new (): PolymerElement & CrScrollableBehavior};

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
        <style>
          #container {
            height: 30px;
            overflow-y: auto;
          }
        </style>
        <div id="container" scrollable>
          <iron-list scroll-target="container" items="[[items]]">
            <template>
              <div>[[item]]</div>
            </template>
          </iron-list>
        </div>
      `;
    }

    static get properties() {
      return {
        items: Array,
      };
    }

    items: string[] = ['apple', 'bannana', 'cucumber', 'doughnut'];
  }
  customElements.define(TestElement.is, TestElement);

  let testElement: TestElement;
  let container: HTMLElement;
  let ironList: IronListElement;

  setup(function(done) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
    container =
        testElement.shadowRoot!.querySelector<HTMLElement>('#container')!;
    ironList = testElement.shadowRoot!.querySelector('iron-list')!;

    // Wait for CrScrollableBehavior to set the initial scrollable class
    // properties.
    window.requestAnimationFrame(() => {
      waitBeforeNextRender(testElement).then(done);
    });
  });

  // There is no MockInteractions scroll event, and simlating a scroll is messy,
  // so instead scroll ironList and send a 'scroll' event to the container.
  function scrollToIndex(index: number) {
    ironList.scrollToIndex(index);
    container.dispatchEvent(new CustomEvent('scroll'));
    flush();
  }

  test('scroll', function() {
    assertTrue(container.classList.contains('can-scroll'));
    assertFalse(container.classList.contains('is-scrolled'));
    assertFalse(container.classList.contains('scrolled-to-bottom'));
    scrollToIndex(1);
    assertTrue(container.classList.contains('is-scrolled'));
    assertFalse(container.classList.contains('scrolled-to-bottom'));
    scrollToIndex(3);
    assertTrue(container.classList.contains('scrolled-to-bottom'));
  });

  test('save scroll', function(done) {
    scrollToIndex(2);
    assertTrue(container.classList.contains('can-scroll'));
    assertTrue(container.classList.contains('is-scrolled'));
    const scrollTop = container.scrollTop;
    testElement.saveScroll(ironList);
    testElement.items = ['apple', 'bannana', 'cactus', 'cucumber', 'doughnut'];
    testElement.restoreScroll(ironList);
    flush();
    window.setTimeout(() => {
      assertEquals(scrollTop, container.scrollTop);
      done();
    });
  });
});
