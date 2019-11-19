// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-scrollable-behavior', function() {
  /** @type {CrScrollableListElement} */ let testElement;
  /** @type {HTMLDivElement} */ let container;
  /** @type {IronListElement} */ let ironList;

  suiteSetup(function() {
    document.body.innerHTML = `
      <dom-module id="test-element">
        <template>
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
        </template>
      </dom-module>
    `;

    Polymer({
      is: 'test-element',

      properties: {
        items: {
          type: Array,
          value: function() {
            return ['apple', 'bannana', 'cucumber', 'doughnut'];
          },
        },
      },

      behaviors: [CrScrollableBehavior],
    });
  });

  setup(function(done) {
    PolymerTest.clearBody();

    testElement = document.createElement('test-element');
    document.body.appendChild(testElement);
    container = testElement.$$('#container');
    ironList = testElement.$$('iron-list');

    // Wait for CrScrollableBehavior to set the initial scrollable class
    // properties.
    window.requestAnimationFrame(() => {
      test_util.waitBeforeNextRender().then(done);
    });
  });

  // There is no MockInteractions scroll event, and simlating a scroll is messy,
  // so instead scroll ironList and send a 'scroll' event to the container.
  function scrollToIndex(index) {
    ironList.scrollToIndex(index);
    container.dispatchEvent(new CustomEvent('scroll'));
    Polymer.dom.flush();
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
    Polymer.dom.flush();
    Polymer.Base.async(function() {
      assertEquals(scrollTop, container.scrollTop);
      done();
    });
  });
});
