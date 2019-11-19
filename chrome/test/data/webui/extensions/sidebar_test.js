// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
import {navigation, Page} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {tap} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise} from '../test_util.m.js';

import {testVisible} from './test_util.js';

window.extension_sidebar_tests = {};
extension_sidebar_tests.suiteName = 'ExtensionSidebarTest';

/** @enum {string} */
extension_sidebar_tests.TestNames = {
  LayoutAndClickHandlers: 'layout and click handlers',
  SetSelected: 'set selected',
};

suite(extension_sidebar_tests.suiteName, function() {
  /** @type {extensions.Sidebar} */
  let sidebar;

  setup(function() {
    PolymerTest.clearBody();
    sidebar = document.createElement('extensions-sidebar');
    document.body.appendChild(sidebar);
  });

  test(assert(extension_sidebar_tests.TestNames.SetSelected), function() {
    const selector = '.section-item.iron-selected';
    expectFalse(!!sidebar.$$(selector));

    window.history.replaceState(undefined, '', '/shortcuts');
    PolymerTest.clearBody();
    sidebar = document.createElement('extensions-sidebar');
    document.body.appendChild(sidebar);
    const whenSelected = eventToPromise('iron-select', sidebar.$.sectionMenu);
    flush();
    return whenSelected
        .then(function() {
          expectEquals(sidebar.$$(selector).id, 'sections-shortcuts');

          window.history.replaceState(undefined, '', '/');
          PolymerTest.clearBody();
          sidebar = document.createElement('extensions-sidebar');
          document.body.appendChild(sidebar);
          const whenSelected =
              eventToPromise('iron-select', sidebar.$.sectionMenu);
          flush();
          return whenSelected;
        })
        .then(function() {
          expectEquals(sidebar.$$(selector).id, 'sections-extensions');
        });
  });

  test(
      assert(extension_sidebar_tests.TestNames.LayoutAndClickHandlers),
      function(done) {
        const boundTestVisible = testVisible.bind(null, sidebar);
        boundTestVisible('#sections-extensions', true);
        boundTestVisible('#sections-shortcuts', true);
        boundTestVisible('#more-extensions', true);

        sidebar.isSupervised = true;
        flush();
        boundTestVisible('#more-extensions', false);

        let currentPage;
        navigation.addListener(newPage => {
          currentPage = newPage;
        });

        tap(sidebar.$$('#sections-shortcuts'));
        expectDeepEquals(currentPage, {page: Page.SHORTCUTS});

        tap(sidebar.$$('#sections-extensions'));
        expectDeepEquals(currentPage, {page: Page.LIST});

        // Clicking on the link for the current page should close the dialog.
        sidebar.addEventListener('close-drawer', () => done());
        tap(sidebar.$$('#sections-extensions'));
      });
});
