// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrScrollableBehavior} from 'chrome://resources/cr_elements/cr_scrollable_behavior.m.js';
import { Base, flush, html,Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';
// clang-format on

suite('cr-scrollable-behavior', function() {
  /** @type {!TestElementElement} */ let testElement;
  /** @type {!HTMLDivElement} */ let container;
  /** @type {!IronListElement} */ let ironList;

  suiteSetup(function() {
    if (window.location.origin === 'chrome://test') {
      // Polymer 3 setup
      Polymer({
        is: 'test-element',

        _template: html`
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
        `,

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
    } else {
      // Polymer 2 setup
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
    }
  });

  setup(function(done) {
    document.body.innerHTML = '';

    testElement = /** @type {!TestElementElement} */ (
        document.createElement('test-element'));
    document.body.appendChild(testElement);
    container = /** @type {!HTMLDivElement} */ (testElement.$$('#container'));
    ironList = /** @type {!IronListElement} */ (testElement.$$('iron-list'));

    // Wait for CrScrollableBehavior to set the initial scrollable class
    // properties.
    window.requestAnimationFrame(() => {
      waitBeforeNextRender(testElement).then(done);
    });
  });

  // There is no MockInteractions scroll event, and simlating a scroll is messy,
  // so instead scroll ironList and send a 'scroll' event to the container.
  function scrollToIndex(index) {
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
    Base.async(function() {
      assertEquals(scrollTop, container.scrollTop);
      done();
    });
  });
});
