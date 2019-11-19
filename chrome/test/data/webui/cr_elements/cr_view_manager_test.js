// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {isVisible} from '../test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for cr-view-manager. */
cr.define('cr_view_manager_test', function() {
  /** @enum {string} */
  const TestNames = {
    Visibility: 'visibility',
    EventFiring: 'event firing',
  };

  let viewManager;
  let parent;
  let views;
  const suiteName = 'CrElementsViewManagerTest';

  suite(suiteName, function() {
    // Initialize an cr-view-manager inside a parent div before
    // each test.
    setup(function() {
      PolymerTest.clearBody();
      document.body.innerHTML = `
        <div id="parent">
          <cr-view-manager id="viewManager">
            <div slot="view" id="viewOne">view 1</div>
            <div slot="view" id="viewTwo">view 2</div>
            <div slot="view" id="viewThree">view 3</div>
          </cr-view-manager>
        </div>
      `;
      parent = document.body.querySelector('#parent');
      viewManager = document.body.querySelector('#viewManager');
    });

    test(assert(TestNames.Visibility), function() {
      function assertViewVisible(id, expectIsVisible) {
        const expectFunc = expectIsVisible ? expectTrue : expectFalse;
        expectFunc(test_util.isVisible(viewManager, '#' + id, true));
      }

      assertViewVisible('viewOne', false);
      assertViewVisible('viewTwo', false);
      assertViewVisible('viewThree', false);

      return viewManager.switchView('viewOne')
          .then(() => {
            assertViewVisible('viewOne', true);
            assertViewVisible('viewTwo', false);
            assertViewVisible('viewThree', false);

            return viewManager.switchView('viewThree');
          })
          .then(() => {
            assertViewVisible('viewOne', false);
            assertViewVisible('viewTwo', false);
            assertViewVisible('viewThree', true);
          });
    });

    test(assert(TestNames.EventFiring), function() {
      const viewOne = viewManager.querySelector('#viewOne');

      const fired = new Set();
      const bubbled = new Set();

      ['view-enter-start', 'view-enter-finish', 'view-exit-start',
       'view-exit-finish',
      ].forEach(type => {
        parent.addEventListener(type, () => {
          bubbled.add(type);
        });
        viewOne.addEventListener(type, () => {
          fired.add(type);
        });
      });

      /**
       * @param {string} eventName The event to check
       * @param {boolean} expectFired Whether the event should have fired.
       */
      function verifyEventFiredAndBubbled(eventName, expectFired) {
        expectEquals(expectFired, fired.has(eventName));
        expectEquals(expectFired, bubbled.has(eventName));
      }

      // Setup the switch promise first.
      let enterPromise = viewManager.switchView('viewOne');
      // view-enter-start should fire synchronously.
      verifyEventFiredAndBubbled('view-enter-start', true);
      // view-enter-finish should not fire yet.
      verifyEventFiredAndBubbled('view-enter-finish', false);
      return enterPromise
          .then(() => {
            // view-enter-finish should fire after animation.
            verifyEventFiredAndBubbled('view-enter-finish', true);

            enterPromise = viewManager.switchView('viewTwo');
            // view-exit-start should fire synchronously.
            verifyEventFiredAndBubbled('view-exit-start', true);
            // view-exit-finish should not fire yet.
            verifyEventFiredAndBubbled('view-exit-finish', false);

            return enterPromise;
          })
          .then(() => {
            // view-exit-finish should fire after animation.
            verifyEventFiredAndBubbled('view-exit-finish', true);
          });
    });
  });

  // #cr_define_end
  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
