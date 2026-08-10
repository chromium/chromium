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
        [routes.SECURITY.path, '#subpageTrigger1'],
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

  test('ParentViewFocusesChildTrigger', async function() {
    const parentView =
        document.createElement('test-parent-view') as TestParentViewElement;
    document.body.appendChild(parentView);

    assertEquals(null, parentView.shadowRoot!.activeElement);

    function assertFocused(id: string) {
      assertEquals(
          parentView.shadowRoot!.querySelector(`#${id}`)!.id,
          parentView.shadowRoot!.activeElement!.id);
    }

    async function simulateNavigateToRouteAndBack(route: Route) {
      Router.getInstance().navigateTo(route);
      const popstate = new Promise<void>(resolve => {
        window.addEventListener('popstate', () => resolve(), {once: true});
      });
      Router.getInstance().navigateToPreviousRoute();
      await popstate;
    }

    // Simulate navigating to the first child route and back to the parent.
    // Manually fire the 'view-enter-start' event, normally fired by the
    // cr-view-manager that hosts all parent and child views.
    await simulateNavigateToRouteAndBack(routes.SECURITY);
    parentView.dispatchEvent(new Event('view-enter-start'));
    assertFocused('subpageTrigger1');

    // Simulate navigating to the second child route and back to the parent.
    await simulateNavigateToRouteAndBack(routes.FONTS);
    parentView.dispatchEvent(new Event('view-enter-start'));
    assertFocused('subpageTrigger2');
  });

  test('ParentViewFocusesChildTriggerOnDirectNavigationBack', function() {
    const parentView =
        document.createElement('test-parent-view') as TestParentViewElement;
    document.body.appendChild(parentView);

    assertEquals(null, parentView.shadowRoot!.activeElement);

    // Simulate direct navigation to a child route (address bar entry leaves
    // history state empty).
    window.history.replaceState(null, '', routes.SECURITY.path);
    Router.getInstance().setCurrentRoute(
        routes.SECURITY, new URLSearchParams(), /*isPopstate=*/ false);

    // Simulate clicking Settings back button (navigates to parent route via
    // fallback).
    Router.getInstance().navigateToPreviousRoute();
    parentView.dispatchEvent(new Event('view-enter-start'));

    assertEquals(
        parentView.shadowRoot!.querySelector('#subpageTrigger1')!.id,
        parentView.shadowRoot!.activeElement!.id);
  });

  test('ChildViewFocusesBackButton', function() {
    const childView =
        document.createElement('test-child-view') as TestChildViewElement;
    document.body.appendChild(childView);

    assertEquals(null, childView.shadowRoot!.activeElement);

    // Simulate navigating to the first child route. Manually fire
    // the 'view-enter-start' event, normally fired by the cr-view-manager that
    // hosts all parent and child views.
    Router.getInstance().navigateTo(routes.SECURITY);
    assertFalse(Router.getInstance().lastRouteChangeWasPopstate());
    childView.dispatchEvent(new Event('view-enter-start'));
    assertEquals(
        childView.shadowRoot!.querySelector('#back')!.id,
        childView.shadowRoot!.activeElement!.id);
  });
});
