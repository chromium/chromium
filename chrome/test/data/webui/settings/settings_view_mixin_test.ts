// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SettingsViewMixin} from 'chrome://settings/settings.js';
import type {Route} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SettingsViewMixin', function() {
  const TestParentViewElementBase = SettingsViewMixin(PolymerElement);

  class TestParentViewElement extends TestParentViewElementBase {
    static get is() {
      return 'test-parent-view';
    }

    static get template() {
      return html`
        <button id="subpageTrigger1">Open child 1</button>
        <button id="subpageTrigger2">Open child 2</button>
      `;
    }

    // Override SettingsViewMixin
    override getFocusConfig() {
      return new Map([
        [routes.SEARCH_ENGINES.path, '#subpageTrigger1'],
        [routes.FONTS.path, '#subpageTrigger2'],

      ]);
    }
  }
  customElements.define(TestParentViewElement.is, TestParentViewElement);

  const TestChildViewElementBase = SettingsViewMixin(PolymerElement);

  class TestChildViewElement extends TestChildViewElementBase {
    static get is() {
      return 'test-child-view';
    }

    static get template() {
      return html`<button id="back">Back</button>`;
    }

    // Override SettingsViewMixin
    override focusBackButton() {
      const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#back');
      assertTrue(!!toFocus);
      toFocus.focus();
    }
  }
  customElements.define(TestChildViewElement.is, TestChildViewElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ParentViewFocusesChildTrigger', function() {
    const parentView =
        document.createElement('test-parent-view') as TestParentViewElement;
    document.body.appendChild(parentView);

    assertEquals(null, parentView.shadowRoot!.activeElement);

    function assertFocused(id: string) {
      assertEquals(
          parentView.shadowRoot!.querySelector(`#${id}`)!.id,
          parentView.shadowRoot!.activeElement!.id);
    }

    function simulateNavigateToRouteAndBack(route: Route) {
      Router.getInstance().navigateTo(route);
      // TODO(dpapad): Figure out why calling navigateToPreviousRoute() does not
      // result lastRouteChangeWasPopstate() being true.
      Router.getInstance().setCurrentRoute(
          routes.BASIC, new URLSearchParams(), /*isPopstate=*/ true);
      assertTrue(Router.getInstance().lastRouteChangeWasPopstate());
    }

    // Simulate navigating to the first child route and back to the parent.
    // Manually fire the 'view-enter-start' event, normally fired by the
    // cr-view-manager that hosts all parent and child views.
    simulateNavigateToRouteAndBack(routes.SEARCH_ENGINES);
    parentView.dispatchEvent(new Event('view-enter-start'));
    assertFocused('subpageTrigger1');

    // Simulate navigating to the second child route and back to the parent.
    simulateNavigateToRouteAndBack(routes.FONTS);
    parentView.dispatchEvent(new Event('view-enter-start'));
    assertFocused('subpageTrigger2');
  });

  test('ChildViewFocusesBackButton', function() {
    const childView =
        document.createElement('test-child-view') as TestChildViewElement;
    document.body.appendChild(childView);

    assertEquals(null, childView.shadowRoot!.activeElement);

    // Simulate navigating to the first child route. Manually fire
    // the 'view-enter-start' event, normally fired by the cr-view-manager that
    // hosts all parent and child views.
    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
    assertFalse(Router.getInstance().lastRouteChangeWasPopstate());
    childView.dispatchEvent(new Event('view-enter-start'));
    assertEquals(
        childView.shadowRoot!.querySelector('#back')!.id,
        childView.shadowRoot!.activeElement!.id);
  });
});
