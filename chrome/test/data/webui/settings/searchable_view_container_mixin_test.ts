// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SearchableViewContainerMixin, SettingsViewMixin} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
        <style>
          cr-view-manager [hidden-by-search],
          cr-view-manager[show-all] [slot=view][data-parent-view-id] {
            display: none;
          }
          cr-view-manager [slot=view]:not(.closing) {
            position: initial;
          }
        </style>
        <div id="container" scrollable>
          <cr-view-manager show-all$="[[shouldShowAll]]">
            <test-parent-view slot="view" id="parentView0">
              <span>ParentView0</span>
              <button data-child-trigger="childView0"></button>
            </test-parent-view>

            <test-parent-view slot="view" id="parentView1">
              <span>ParentView1</span>
              <button data-child-trigger="childView1"></button>
            </test-parent-view>

            <div slot="view" id="childView0" data-parent-view-id="parentView0">
              ChildView0
            </div>
            <div slot="view" id="childView1" data-parent-view-id="parentView1">
              ChildView1
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
        assertEquals(parentVisible[i], isVisible(parentViews[i]!));
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

    testElement.inSearchMode = true;

    // Case1: Search results exist only in #parentView0.
    let result = await testElement.searchContents('ParentView0');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [false, false]);

    // Case2: Search results exist only in #parentView1.
    result = await testElement.searchContents('ParentView1');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, false]);

    // Case3: Search results exist in both #parentView0, #parentView1.
    result = await testElement.searchContents('ParentView');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, true], [false, false]);

    // Case4: Search results exist in both #parentView0, #childView0.
    result = await testElement.searchContents('View0');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [true, false]);

    // Case5: Search results exist only #childView0.
    result = await testElement.searchContents('ChildView0');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([true, false], [true, false]);

    // Case6: Search results exist in both #parentView1, #childView1.
    result = await testElement.searchContents('View1');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, true]);

    // Case7: Search results exist only #childView1.
    result = await testElement.searchContents('ChildView1');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, true]);

    // Case8: Search results exist only in #childView0, #childView1.
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

    // Case10: childView0 has 'no-search'.
    childViews[0]!.toggleAttribute('no-search', true);
    result = await testElement.searchContents('ChildView');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, true], [false, true]);

    // Case11: childView1 also has 'no-search'.
    childViews[1]!.toggleAttribute('no-search', true);
    // Clear search first and then search again for 'ChildView'.
    await testElement.searchContents('');
    result = await testElement.searchContents('ChildView');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertDomState([false, false], [false, false]);
  });
});
