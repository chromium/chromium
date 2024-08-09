// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrLitElement, html as litHtml} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {PolymerElement, html as polymerHtml} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// clang-format on


let viewManager: CrViewManagerElement;
let parent: HTMLElement;

function assertViewVisible(id: string, expectIsVisible: boolean) {
  assertEquals(
      expectIsVisible,
      isChildVisible(viewManager, `#${id}`, /*checkLightDom=*/ true));
}

suite('CrElementsViewManagerTest', function() {
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

suite('CrLazyRenderInCrViewManagerTest', function() {
  class TestApp extends PolymerElement {
    static get is() {
      return 'test-app';
    }

    static get template() {
      return polymerHtml`
        <cr-view-manager id="viewManager">
          <div slot="view" id="viewOne">view one</div>
          <cr-lazy-render id="lazy">
            <template>
              <div slot="view" id="lazyView"></div>
            </template>
          </cr-lazy-render>
        </cr-view-manager>`;
    }
  }

  class TestAppLit extends CrLitElement {
    static get is() {
      return 'test-app-lit';
    }

    override render() {
      return litHtml`
        <cr-view-manager id="viewManager">
          <div slot="view" id="viewOne">view one</div>
          <cr-lazy-render-lit id="lazy"
              .template="${
          () => litHtml`<div slot="view" id="lazyView"></div>`}">
          </cr-lazy-render-lit>
        </cr-view-manager>`;
    }
  }

  customElements.define(TestApp.is, TestApp);
  customElements.define(TestAppLit.is, TestAppLit);

  function setupTest(isLit: boolean) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testApp = isLit ? document.createElement('test-app-lit') :
                            document.createElement('test-app');
    document.body.appendChild(testApp);

    viewManager = testApp.shadowRoot!.querySelector('#viewManager')!;
    assertTrue(!!viewManager);
  }

  test('switch to cr-lazy-render view', async function() {
    setupTest(/*isLit=*/ false);

    assertViewVisible('viewOne', false);
    assertViewVisible('lazyView', false);

    await viewManager.switchView('viewOne');
    assertViewVisible('viewOne', true);
    assertViewVisible('lazyView', false);

    await viewManager.switchView('lazy');
    assertViewVisible('viewOne', false);
    assertViewVisible('lazyView', true);
  });

  test('switch to cr-lazy-render-lit view', async function() {
    setupTest(/*isLit=*/ true);

    assertViewVisible('viewOne', false);
    assertViewVisible('lazyView', false);

    await viewManager.switchView('viewOne');
    assertViewVisible('viewOne', true);
    assertViewVisible('lazyView', false);

    await viewManager.switchView('lazy');
    assertViewVisible('viewOne', false);
    assertViewVisible('lazyView', true);
  });
});
