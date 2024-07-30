// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// clang-format on


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

  test('visibility', async function() {
    function assertViewVisible(id: string, expectIsVisible: boolean) {
      assertEquals(
          expectIsVisible,
          isChildVisible(viewManager, `#${id}`, /*checkLightDom=*/ true));
    }

    assertViewVisible('viewOne', false);
    assertViewVisible('viewTwo', false);
    assertViewVisible('viewThree', false);

    await viewManager.switchView('viewOne');
    assertViewVisible('viewOne', true);
    assertViewVisible('viewTwo', false);
    assertViewVisible('viewThree', false);

    await viewManager.switchView('viewThree');
    assertViewVisible('viewOne', false);
    assertViewVisible('viewTwo', false);
    assertViewVisible('viewThree', true);
  });

  test('event firing', async function() {
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

    await exitPromises;
    verifyEventFiredAndBubbled('view-exit-finish', true);

    fired = new Set();
    bubbled = new Set();

    // Switching back has an animation this time.
    const enterPromises = viewManager.switchView('viewOne');
    verifyEventFiredAndBubbled('view-enter-start', true);
    verifyEventFiredAndBubbled('view-enter-finish', false);
    await enterPromises;
    verifyEventFiredAndBubbled('view-enter-finish', true);
  });
});
