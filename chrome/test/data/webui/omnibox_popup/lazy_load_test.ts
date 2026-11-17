// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally do not statically import
// 'chrome://omnibox-popup.top-chrome/lazy_load.js' or
// 'chrome://omnibox-popup.top-chrome/omnibox_popup_full.js' at the top level
// so this test can verify which custom elements are registered by
// omnibox_popup_full.js vs. lazy_load.js.
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

// Inventory of custom elements registered by the initial critical-path bundles
// (omnibox_popup_full.rollup.js and shared.rollup.js) vs. lazy_load.rollup.js.
//
// Why list all transitive custom elements (including cr-* sub-components)?
// 1. Visibility into further lazy-loading opportunities: It makes explicit
//    which internal sub-components (such as <cr-searchbox-dropdown> or
//    <cr-searchbox-match>) are currently compiled into the eager bundle
//    alongside larger wrapper components and are candidates for future lazy
//    loading.
// 2. Preventing silent bundle erosion: A small import change in an eager file
//    can transitively pull a heavy sub-component tree (e.g. <help-bubble> or
//    <cr-composebox-contextual-entrypoint-button>) into shared.rollup.js.
// 3. Low maintenance overhead: On failure, this test evaluates all elements in
//    EAGER_ELEMENTS and LAZY_ELEMENTS before asserting and prints the complete
//    diff of leaked/missing elements at once. Updating the list (manually or
//    via a coding agent) is trivial, while surfacing the diff in code review
//    gives an expert reviewer the context needed to decide whether a change is
//    expected or whether lazy loading has eroded.
//
// LINT.IfChange(LazyLoadedModules)
const EAGER_ELEMENTS = [
  'cr-a11y-announcer',
  'cr-button',
  'cr-icon',
  'cr-icon-button',
  'cr-ripple',
  'cr-searchbox-action',
  'cr-searchbox-compose-button',
  'cr-searchbox-dropdown',
  'cr-searchbox-icon',
  'cr-searchbox-input',
  'cr-searchbox-match',
  'omnibox-full-app',
  'omnibox-popup-searchbox',
];

// If an element in LAZY_ELEMENTS fails this check, an eager module in the
// omnibox_popup_full.ts graph transitively imported it into shared.rollup.js
// (for example, by importing a runtime enum/constant/helper from the element
// file instead of a shared non-component module, using a value import instead
// of `import type`, or adding a top-level side-effect import in an eager file
// instead of lazy_load.ts).
const LAZY_ELEMENTS = [
  'composebox-current-tab-chip',
  'composebox-favicon-group',
  'cr-composebox-contextual-entrypoint-button',
  'cr-composebox-lens-search',
  'cr-iconset',
  'help-bubble',
  'omnibox-popup-contextual-entrypoint',
  'omnibox-popup-contextual-entrypoint-button',
];
// LINT.ThenChange(
//     //chrome/browser/resources/omnibox_popup/full/lazy_load.ts:LazyLoadedModules,
//     //chrome/browser/resources/omnibox_popup/full/omnibox_popup_searchbox.ts:LazyLoadedModules
// )

suite('OmniboxPopupFullLazyLoadTest', () => {
  test('lazy elements are only defined after ensureLazyLoaded', async () => {
    const {WindowProxy} =
        await import('chrome://omnibox-popup.top-chrome/omnibox_popup_full.js');

    const missingFromEager =
        EAGER_ELEMENTS.filter(tag => !customElements.get(tag));
    const leakedIntoEager =
        LAZY_ELEMENTS.filter(tag => Boolean(customElements.get(tag)));

    await WindowProxy.getInstance().ensureLazyLoaded();

    const missingAfterLazyLoad =
        LAZY_ELEMENTS.filter(tag => !customElements.get(tag));

    const errors: string[] = [];
    if (leakedIntoEager.length > 0) {
      errors.push(
          `Lazy elements leaked into omnibox_popup_full.js / ` +
          `shared.rollup.js:\n` +
          leakedIntoEager.map(t => `  - <${t}>`).join('\n'));
    }
    if (missingFromEager.length > 0) {
      errors.push(
          `Expected eager elements not defined by omnibox_popup_full.js ` +
          `(if intentionally lazy-loaded or removed, move to LAZY_ELEMENTS ` +
          `or remove from EAGER_ELEMENTS):\n` +
          missingFromEager.map(t => `  - <${t}>`).join('\n'));
    }
    if (missingAfterLazyLoad.length > 0) {
      errors.push(
          `Expected lazy elements still not defined after lazy_load.js ` +
          `(if removed, delete from LAZY_ELEMENTS):\n` +
          missingAfterLazyLoad.map(t => `  - <${t}>`).join('\n'));
    }

    assertDeepEquals([], errors, '\n' + errors.join('\n\n'));
  });
});
