// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally do not statically import 'chrome://new-tab-page/lazy_load.js'
// or 'chrome://new-tab-page/new_tab_page.js' at the top level so this test can
// verify which custom elements are registered by new_tab_page.js vs.
// lazy_load.js.
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

// Inventory of custom elements registered by the initial critical-path bundles
// (new_tab_page.rollup.js and shared.rollup.js) vs. lazy_load.rollup.js.
//
// Why list all transitive custom elements (including cr-* sub-components)?
// 1. Visibility into further lazy-loading opportunities: It makes explicit
//    which internal sub-components (such as <cr-searchbox-dropdown>,
//    <cr-composebox-contextual-action-menu>, or <cr-action-menu>) are
//    currently compiled into the eager bundle alongside larger wrapper
//    components and are candidates for future lazy loading.
// 2. Preventing silent bundle erosion: A small import change in an eager file
//    can transitively pull a heavy sub-component tree (e.g. <cr-dialog>,
//    <cr-input>) into shared.rollup.js.
// 3. Low maintenance overhead: On failure, this test evaluates all elements in
//    EAGER_ELEMENTS and LAZY_ELEMENTS before asserting and prints the complete
//    diff of leaked/missing elements at once. Updating the list (manually or
//    via a coding agent) is trivial, while surfacing the diff in code review
//    gives an expert reviewer the context needed to decide whether a change is
//    expected or whether lazy loading has eroded.
//
// LINT.IfChange(LazyLoadedModules)
const EAGER_ELEMENTS = [
  'composebox-favicon-group',
  'cr-a11y-announcer',
  'cr-action-menu',
  'cr-button',
  'cr-composebox-contextual-action-menu',
  'cr-composebox-contextual-entrypoint-and-menu',
  'cr-composebox-contextual-entrypoint-button',
  'cr-composebox-file-inputs',
  'cr-composebox-tab-favicon',
  'cr-icon',
  'cr-icon-button',
  'cr-iconset',
  'cr-left-hand-side',
  'cr-ripple',
  'cr-searchbox-action',
  'cr-searchbox-compose-button',
  'cr-searchbox-dropdown',
  'cr-searchbox-icon',
  'cr-searchbox-input',
  'cr-searchbox-match',
  'cr-threads-rail',
  'cr-tooltip',
  'help-bubble',
  'ntp-app',
  'ntp-iframe',
  'ntp-logo',
  'ntp-searchbox',
];

// If an element in LAZY_ELEMENTS fails this check, an eager module in the
// new_tab_page.ts graph transitively imported it into shared.rollup.js (for
// example, by importing a runtime enum/constant/helper from the element file
// instead of a shared non-component module like common.ts or metrics_utils.ts,
// using a value import instead of `import type`, or adding a top-level
// side-effect import in an eager file instead of lazy_load.ts).
const LAZY_ELEMENTS = [
  'audio-wave',
  'cr-auto-img',
  'cr-chip',
  'cr-composebox',
  'cr-composebox-aim-button',
  'cr-composebox-dropdown',
  'cr-composebox-file-carousel',
  'cr-composebox-file-thumbnail',
  'cr-composebox-input',
  'cr-composebox-lens-search',
  'cr-composebox-match',
  'cr-composebox-submit',
  'cr-composebox-tool-chip',
  'cr-composebox-voice-search',
  'cr-dialog',
  'cr-grid',
  'cr-input',
  'cr-most-visited',
  'cr-page-selector',
  'cr-policy-indicator',
  'cr-toast',
  'cr-toast-manager',
  'cr-tooltip-icon',
  'individual-promos',
  'localized-link',
  'ntp-action-chips',
  'ntp-calendar',
  'ntp-calendar-event',
  'ntp-composebox',
  'ntp-customize-buttons',
  'ntp-doodle-share-dialog',
  'ntp-drive-module',
  // <if expr="not is_official_build">
  'ntp-dummy-module',
  // </if>
  'ntp-error-scrim',
  'ntp-file-suggestion',
  'ntp-google-calendar-module',
  'ntp-icon-container',
  'ntp-info-dialog',
  'ntp-lens-form',
  'ntp-lens-upload-dialog',
  'ntp-microsoft-auth-module',
  'ntp-microsoft-files-module',
  'ntp-module-header',
  'ntp-module-wrapper',
  'ntp-modules',
  'ntp-most-relevant-tab-resumption-module',
  'ntp-outlook-calendar-module',
  'ntp-tab-groups-module',
  'ntp-voice-search-overlay',
  'page-favicon',
  'recording-wave',
  'search-animated-glow',
];
// LINT.ThenChange(
//     //chrome/browser/resources/new_tab_page/lazy_load.ts:LazyLoadedModules,
//     //chrome/browser/resources/new_tab_page/new_tab_page_eager.ts:LazyLoadedModules
// )

suite('NewTabPageLazyLoadTest', () => {
  test('lazy elements are only defined after ensureLazyLoaded', async () => {
    const {WindowProxy} = await import('chrome://new-tab-page/new_tab_page.js');

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
          `Lazy elements leaked into new_tab_page.js / shared.rollup.js:\n` +
          leakedIntoEager.map(t => `  - <${t}>`).join('\n'));
    }
    if (missingFromEager.length > 0) {
      errors.push(
          `Expected eager elements not defined by new_tab_page.js ` +
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
