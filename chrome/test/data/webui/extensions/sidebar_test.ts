// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
import {ExtensionsSidebarElement, navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {testVisible} from './test_util.js';

suite('ExtensionSidebarTest', function() {
  let sidebar: ExtensionsSidebarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('extensions-sidebar');
    sidebar.enableEnhancedSiteControls = false;
    document.body.appendChild(sidebar);
  });

  test('SetSelected', function() {
    const selector = '.cr-nav-menu-item.iron-selected';
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
      'LayoutAndClickHandlers', function(done) {
        const boundTestVisible = testVisible.bind(null, sidebar);
        boundTestVisible('#sectionsExtensions', true);

        // The site permissions link should not be visible if
        // enableEnhancedSiteControls is set to false.
        boundTestVisible('#sectionsSitePermissions', false);
        boundTestVisible('#sectionsShortcuts', true);
        boundTestVisible('#moreExtensions', true);

        sidebar.enableEnhancedSiteControls = true;
        flush();
        boundTestVisible('#sectionsSitePermissions', true);

        let currentPage;
        navigation.addListener(newPage => {
          currentPage = newPage;
        });

        sidebar.$.sectionsShortcuts.click();
        assertDeepEquals(currentPage, {page: Page.SHORTCUTS});

        sidebar.$.sectionsExtensions.click();
        assertDeepEquals(currentPage, {page: Page.LIST});

        sidebar.$.sectionsSitePermissions.click();
        assertDeepEquals(currentPage, {page: Page.SITE_PERMISSIONS});

        // Clicking on the link for the current page should close the dialog.
        sidebar.addEventListener('close-drawer', () => done());
        sidebar.$.sectionsExtensions.click();
      });


  test('HrefVerification', function(done) {
    sidebar.enableEnhancedSiteControls = true;
    flush();
    assertEquals('/', sidebar.$.sectionsExtensions.getAttribute('href'));
    assertEquals(
        '/sitePermissions',
        sidebar.$.sectionsSitePermissions.getAttribute('href'));
    assertEquals(
        '/shortcuts', sidebar.$.sectionsShortcuts.getAttribute('href'));
    assertTrue(sidebar.$.moreExtensions.querySelector('a')!
                   .getAttribute('href')!.includes('utm_source=ext_sidebar'));
    done();
  });
});
