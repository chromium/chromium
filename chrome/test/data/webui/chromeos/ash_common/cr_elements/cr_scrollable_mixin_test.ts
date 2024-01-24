// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

// clang-format on

suite('cr-scrollable-mixin', function() {
  const TestElementBase = CrScrollableMixin(PolymerElement);

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

suite('cr-scrollable-mixin items', function() {
  const TestElementBase = CrScrollableMixin(PolymerElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-items-element';
    }

    static get template() {
      return html`
        <style>
          .hidden {
            display: none;
          }

          #container {
            min-height: 1px;
          }
        </style>
        <div id="outer" class="hidden">
          <div id="container" scrollable>
            <iron-list id="list" scroll-target="container" items="[[items]]">
              <template>
                <div class="item">[[item]]</div>
              </template>
            </iron-list>
          </div>
        </div>
      `;
    }

    static get properties() {
      return {
        items: Array,
        opened: Boolean,
      };
    }

    items: string[] = ['apple', 'bannana', 'cucumber', 'doughnut', 'enchilada'];
    opened: boolean = false;
  }
  customElements.define(TestElement.is, TestElement);

  let testElement: TestElement;
  let ironList: IronListElement;

  setup(function(done) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('test-items-element') as TestElement;
    document.body.appendChild(testElement);
    ironList = testElement.shadowRoot!.querySelector('iron-list')!;

    // Wait for CrScrollableBehavior to set the initial scrollable class
    // properties.
    window.requestAnimationFrame(() => {
      waitBeforeNextRender(testElement).then(done);
    });
  });

  test('initially hidden', function(done) {
    const outer = testElement.shadowRoot!.querySelector('#outer')!;
    assertEquals(
        0, ironList.shadowRoot!.querySelectorAll('.item')!.length,
        'should have no initial items');
    let resizeEvents = 0;
    const resizeListener = function() {
      flush();
      // There will be two resize events - the first is fired after scrollHeight
      // changes from 0 -> 1, which updates scrollHeight to its final value.
      // Then, the second resize event is fired causing the list to properly
      // render its items.
      resizeEvents += 1;
      if (resizeEvents === 1) {
        assertEquals(
            3,
            testElement.shadowRoot!.querySelectorAll('.item')!.length,
            'should have default minimum number of items',
        );
      } else if (resizeEvents === 2) {
        assertEquals(
            testElement.items.length,
            testElement.shadowRoot!.querySelectorAll('.item')!.length,
            'should render all items',
        );
        done();
      }
    };
    ironList.addEventListener('iron-resize', resizeListener);
    testElement.updateScrollableContents();
    window.setTimeout(function() {
      outer.classList.remove('hidden');
    }, 100);
  });
});
