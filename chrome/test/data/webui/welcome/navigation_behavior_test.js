// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {navigateTo, navigateToNextStep, NavigationMixin, Routes} from 'chrome://welcome/navigation_behavior.js';

import {eventToPromise} from '../test_util.m.js';

suite('NavigationBehaviorTest', function() {
  let elements = [];
  let callOrders = [];

  suiteSetup(function() {
    class TestElement extends NavigationMixin
    (PolymerElement) {
      static get is() {
        return 'test-element';
      }

      static get properties() {
        return {
          subtitle: {
            type: String,
            value: 'My subtitle',
          },
        };
      }

      static get template() {
        return html``;
      }

      ready() {
        super.ready();
        this.reset();
      }

      onRouteEnter() {
        this.enterCalled = true;
        callOrders.push('enter');
      }

      onRouteChange() {
        this.changeCalled = true;
        callOrders.push('change');
      }

      onRouteExit() {
        this.exitCalled = true;
        callOrders.push('exit');
      }

      reset() {
        this.enterCalled = false;
        this.changeCalled = false;
        this.exitCalled = false;
      }
    }
    customElements.define(TestElement.is, TestElement);
  });

  setup(function() {
    loadTimeData.overrideValues({
      headerText: 'My title',
    });


    document.body.innerHTML = '';
    // Creates 3 elements with IDs step-(0~2).
    for (let i = 0; i < 3; i++) {
      elements.push(document.createElement('test-element'));
      elements[i].id = `step-${i}`;
      elements[i].subtitle = `Step ${i}`;
    }
  });

  teardown(function() {
    callOrders = [];
    elements = [];
  });

  function appendAll() {
    elements.forEach(elem => document.body.appendChild(elem));
  }

  function resetAll() {
    elements.forEach(elem => elem.reset());
    callOrders = [];
  }

  // exit should be called first, enter last, and all change calls in between.
  function assertCallOrders() {
    assertEquals(callOrders[0], 'exit');
    assertEquals(callOrders[callOrders.length - 1], 'enter');
    callOrders.slice(1, callOrders.length - 1).forEach(called => {
      assertEquals(called, 'change');
    });
  }

  test('correct hooks fire when elements are attached', function() {
    // Setup the "current route" state before things are appended.
    navigateTo(/* doesn't matter which route */ Routes.NEW_USER, 1);
    appendAll();

    assertFalse(elements[0].enterCalled);
    assertTrue(elements[0].changeCalled);
    assertFalse(elements[0].exitCalled);

    assertTrue(elements[1].enterCalled);
    assertTrue(elements[1].changeCalled);
    assertFalse(elements[1].exitCalled);

    assertFalse(elements[2].enterCalled);
    assertTrue(elements[2].changeCalled);
    assertFalse(elements[2].exitCalled);
  });

  test('hooks fire in expected order when elements are attached', function() {
    // Pretend we're on step-1
    navigateTo(/* doesn't matter which route */ Routes.NEW_USER, 1);
    appendAll();
    resetAll();

    // move on from step-1 to step 2.
    navigateToNextStep();

    assertFalse(elements[0].enterCalled);
    assertTrue(elements[0].changeCalled);
    assertFalse(elements[0].exitCalled);

    assertFalse(elements[1].enterCalled);
    assertTrue(elements[1].changeCalled);
    assertTrue(elements[1].exitCalled);

    assertTrue(elements[2].enterCalled);
    assertTrue(elements[2].changeCalled);
    assertFalse(elements[2].exitCalled);
    assertCallOrders();
  });

  test('popstate works as expected', async function() {
    // Pretend we're on step-1
    navigateTo(/* doesn't matter which route */ Routes.NEW_USER, 1);
    appendAll();
    // move on from step-1 to step 2.
    navigateToNextStep();
    resetAll();

    // back from step-2 to step 1.
    window.history.back();

    await eventToPromise('popstate', window);

    assertFalse(elements[0].enterCalled);
    assertTrue(elements[0].changeCalled);
    assertFalse(elements[0].exitCalled);

    assertTrue(elements[1].enterCalled);
    assertTrue(elements[1].changeCalled);
    assertFalse(elements[1].exitCalled);

    assertFalse(elements[2].enterCalled);
    assertTrue(elements[2].changeCalled);
    assertTrue(elements[2].exitCalled);

    assertCallOrders();

    resetAll();
    // move on from step-1 to step 2 again.
    window.history.forward();

    await eventToPromise('popstate', window);

    assertFalse(elements[0].enterCalled);
    assertTrue(elements[0].changeCalled);
    assertFalse(elements[0].exitCalled);

    assertFalse(elements[1].enterCalled);
    assertTrue(elements[1].changeCalled);
    assertTrue(elements[1].exitCalled);

    assertTrue(elements[2].enterCalled);
    assertTrue(elements[2].changeCalled);
    assertFalse(elements[2].exitCalled);
    assertCallOrders();
  });

  test('updates title', () => {
    navigateTo(/* doesn't matter which route */ Routes.NEW_USER, 0);
    appendAll();
    resetAll();
    assertEquals('My title - Step 0', document.title);

    navigateTo(/* doesn't matter which route */ Routes.NEW_USER, 1);
    assertEquals('My title - Step 1', document.title);
  });
});
