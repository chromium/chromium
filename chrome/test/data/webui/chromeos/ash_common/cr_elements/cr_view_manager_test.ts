// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// clang-format on


/** @fileoverview Suite of tests for cr-view-manager. */
/** @enum {string} */
enum TestNames {
  VISIBILITY = 'visibility',
  EVENT_FIRING = 'event firing',
}

let viewManager: CrViewManagerElement;
let parent: HTMLElement;

const suiteName = 'CrElementsViewManagerTest';

suite(suiteName, function() {
  // Initialize an cr-view-manager inside a parent div before
  // each test.
  setup(function() {
    document.body.innerHTML = getTrustedHTML`
        <div id="parent">
          <cr-view-manager id="viewManager">
            <div slot="view" id="viewOne">view 1</div>
            <div slot="view" id="viewTwo">view 2</div>
            <div slot="view" id="viewThree">view 3</div>
          </cr-view-manager>
        </div>
      `;
    parent = document.body.querySelector('#parent')!;
    viewManager = document.body.querySelector('#viewManager')!;
  });

  test(TestNames.VISIBILITY, function() {
    function assertViewVisible(id: string, expectIsVisible: boolean) {
      assertEquals(
          expectIsVisible,
          isChildVisible(viewManager, `#${id}`, /*checkLightDom=*/ true));
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

  test(TestNames.EVENT_FIRING, function() {
    const viewOne = viewManager.querySelector('#viewOne')!;

    let fired = new Set();
    let bubbled = new Set();

    ['view-enter-start',
     'view-enter-finish',
     'view-exit-start',
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
     * @param eventName The event to check
     * @param expectFired Whether the event should have fired.
     */
    function verifyEventFiredAndBubbled(
        eventName: string, expectFired: boolean) {
      assertEquals(expectFired, fired.has(eventName));
      assertEquals(expectFired, bubbled.has(eventName));
    }

    // Initial switch has no animation.
    viewManager.switchView('viewOne');
    // view-enter-start and view-enter-finish are fired synchronously when
    // there's no animation.
    verifyEventFiredAndBubbled('view-enter-start', true);
    verifyEventFiredAndBubbled('view-enter-finish', true);

    const exitPromises = viewManager.switchView('viewTwo');
    verifyEventFiredAndBubbled('view-exit-start', true);
    // view-exit-finish is waiting on the animation.
    verifyEventFiredAndBubbled('view-exit-finish', false);

    return exitPromises
        .then(() => {
          verifyEventFiredAndBubbled('view-exit-finish', true);

          fired = new Set();
          bubbled = new Set();

          // Switching back has an animation this time.
          const enterPromises = viewManager.switchView('viewOne');
          verifyEventFiredAndBubbled('view-enter-start', true);
          verifyEventFiredAndBubbled('view-enter-finish', false);
          return enterPromises;
        })
        .then(() => {
          verifyEventFiredAndBubbled('view-enter-finish', true);
        });
  });
});
