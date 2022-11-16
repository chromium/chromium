// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
import {ExtensionsSidebarElement, navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {testVisible} from './test_util.js';

const extension_sidebar_tests = {
  suiteName: 'ExtensionSidebarTest',
  TestNames: {
    LayoutAndClickHandlers: 'layout and click handlers',
    SetSelected: 'set selected',
  },
};

Object.assign(window, {extension_sidebar_tests});

suite(extension_sidebar_tests.suiteName, function() {
  let sidebar: ExtensionsSidebarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('extensions-sidebar');
    sidebar.enableEnhancedSiteControls = false;
    document.body.appendChild(sidebar);
  });

  test(extension_sidebar_tests.TestNames.SetSelected, function() {
    const selector = '.section-item.iron-selected';
    assertFalse(!!sidebar.shadowRoot!.querySelector(selector));

    window.history.replaceState(undefined, '', '/shortcuts');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('extensions-sidebar');
    document.body.appendChild(sidebar);
    const whenSelected = eventToPromise('iron-select', sidebar.$.sectionMenu);
    flush();
    return whenSelected
        .then(function() {
          assertEquals(
              sidebar.shadowRoot!.querySelector(selector)!.id,
              'sectionsShortcuts');

          window.history.replaceState(undefined, '', '/');
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          sidebar = document.createElement('extensions-sidebar');
          document.body.appendChild(sidebar);
          const whenSelected =
              eventToPromise('iron-select', sidebar.$.sectionMenu);
          flush();
          return whenSelected;
        })
        .then(function() {
          assertEquals(
              sidebar.shadowRoot!.querySelector(selector)!.id,
              'sectionsExtensions');
        });
  });

  test(
      extension_sidebar_tests.TestNames.LayoutAndClickHandlers, function(done) {
        const boundTestVisible = testVisible.bind(null, sidebar);
        boundTestVisible('#sectionsExtensions', true);

        // The site permissions link should not be visible if
        // enableEnhancedSiteControls is set to false.
        boundTestVisible('#sections-site-permissions', false);
        boundTestVisible('#sectionsShortcuts', true);
        boundTestVisible('#more-extensions', true);

        sidebar.enableEnhancedSiteControls = true;
        flush();
        boundTestVisible('#sections-site-permissions', true);

        let currentPage;
        navigation.addListener(newPage => {
          currentPage = newPage;
        });

        sidebar.$.sectionsShortcuts.click();
        assertDeepEquals(currentPage, {page: Page.SHORTCUTS});

        sidebar.$.sectionsExtensions.click();
        assertDeepEquals(currentPage, {page: Page.LIST});

        sidebar.shadowRoot!
            .querySelector<HTMLElement>('#sections-site-permissions')!.click();
        assertDeepEquals(currentPage, {page: Page.SITE_PERMISSIONS});

        // Clicking on the link for the current page should close the dialog.
        sidebar.addEventListener('close-drawer', () => done());
        sidebar.$.sectionsExtensions.click();
      });
});
