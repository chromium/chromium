// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SearchableViewContainerMixin, SettingsViewMixin} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SearchableViewContainerMixin', function() {
  const TestParentViewElementBase = SettingsViewMixin(PolymerElement);

  class TestParentViewElement extends TestParentViewElementBase {
    static get is() {
      return 'test-parent-view';
    }

    static get template() {
      return html`<slot></slot>`;
    }

    // Override SettingsViewMixin
    override getAssociatedControlFor(childId: string) {
      const trigger =
          this.querySelector<HTMLElement>(`[data-child-trigger="${childId}`);
      assertTrue(!!trigger);
      return trigger;
    }
  }
  customElements.define(TestParentViewElement.is, TestParentViewElement);

  const TestElementBase = SearchableViewContainerMixin(PolymerElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
        <div id="container" scrollable>
          <cr-view-manager show-all$="[[shouldShowAll]]">
            <test-parent-view slot="view" id="parentView1">
              <span>ParentView1</span>
              <button data-child-trigger="childView1"></button>
            </test-parent-view>

            <test-parent-view slot="view" id="parentView2">
              <span>ParentView2</span>
              <button data-child-trigger="childView2"></button>
            </test-parent-view>

            <div slot="view" id="childView1" data-parent-view-id="parentView1">
              ChildView1
            </div>
            <div slot="view" id="childView2" data-parent-view-id="parentView2">
              ChildView2
            </div>
          </cr-view-manager>
        </div>
      `;
    }
  }
  customElements.define(TestElement.is, TestElement);

  let testElement: TestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    Router.getInstance().navigateTo(routes.BASIC);
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  test('shouldShowAll', function() {
    function assertShowAll(showAll: boolean) {
      assertEquals(
          showAll,
          testElement.shadowRoot!.querySelector('cr-view-manager')!
              .hasAttribute('show-all'));
    }

    assertShowAll(false);

    testElement.inSearchMode = true;
    assertShowAll(true);

    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
    assertShowAll(false);

    Router.getInstance().navigateTo(routes.BASIC);
    assertShowAll(true);

    testElement.inSearchMode = false;
    assertShowAll(false);
  });

  test('searchContents', async function() {
    const parentViews = testElement.shadowRoot!.querySelectorAll<HTMLElement>(
        'test-parent-view[slot=view]');
    assertEquals(2, parentViews.length);
    const childViews = testElement.shadowRoot!.querySelectorAll<HTMLElement>(
        '[slot=view][data-parent-view-id]');
    assertEquals(2, childViews.length);

    function assertDomState(
        parentVisible: [boolean, boolean], childHasHits: [boolean, boolean]) {
      // Check parent views hidden-by-search attribute.
      for (let i = 0; i < parentVisible.length; i++) {
        assertEquals(
            parentVisible[i],
            !parentViews[i]!.hasAttribute('hidden-by-search'));
      }

      // Check whether search highlights exist within child views.
      for (let i = 0; i < childHasHits.length; i++) {
        assertEquals(
            childHasHits[i],
            !!childViews[i]!.querySelector('.search-highlight-wrapper'));
      }

      // Check whether search bubbles pointing to the child entry points exist
      // on the parent.
      for (let i = 0; i < childHasHits.length; i++) {
        assertEquals(
            childHasHits[i],
            !!parentViews[i]!.querySelector(
                '[data-child-trigger] .search-bubble'));
      }
    }

    // Case1: Search results exist only in #parentView1.
    let result = await testElement.searchContents('ParentView1');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [false, false]);

    // Case2: Search results exist only in #parentView2.
    result = await testElement.searchContents('ParentView2');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, false]);

    // Case3: Search results exist in both #parentView1, #parentView2.
    result = await testElement.searchContents('ParentView');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, true], [false, false]);

    // Case4: Search results exist in both #parentView1, #childView1.
    result = await testElement.searchContents('View1');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [true, false]);

    // Case5: Search results exist only #childView1.
    result = await testElement.searchContents('ChildView1');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [true, false]);

    // Case6: Search results exist in both #parentView2, #childView2.
    result = await testElement.searchContents('View2');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, true]);

    // Case7: Search results exist only #childView2.
    result = await testElement.searchContents('ChildView2');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, true]);

    // Case8: Search results exist only in #childView1, #childView2.
    result = await testElement.searchContents('ChildView');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, true], [true, true]);

    // Case9: Clear search.
    result = await testElement.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
    assertDomState([true, true], [false, false]);
  });
});
