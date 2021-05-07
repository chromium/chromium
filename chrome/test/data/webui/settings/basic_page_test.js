// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// clang-format off
import 'chrome://settings/settings.js';

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData, pageVisibility, Router, routes} from 'chrome://settings/settings.js';

import {flushTasks, isVisible} from '../test_util.m.js';
// clang-format on

// Register mocha tests.
suite('SettingsBasicPage', function() {
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('basic pages', function() {
    const sections = [
      'appearance', 'onStartup', 'people', 'search', 'autofill', 'safetyCheck',
      'privacy'
    ];
    if (!isChromeOS) {
      sections.push('defaultBrowser');
    }

    flush();

    for (const section of sections) {
      const sectionElement = page.$$(`settings-section[section=${section}]`);
      assertTrue(!!sectionElement);
    }
  });

  test('safetyCheckVisibilityTest', function() {
    // Set the visibility of the pages under test to "false".
    page.pageVisibility = Object.assign(pageVisibility || {}, {
      safetyCheck: false,
    });
    flush();

    const sectionElement = page.$$('settings-section-safety-check');
    assertFalse(!!sectionElement);
  });
});

suite('SettingsBasicPageRedesign', () => {
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    page.scroller = document.body;
  });

  /** @param {string} section */
  function assertActiveSection(section) {
    const activeSections =
        page.shadowRoot.querySelectorAll('settings-section[active]');
    assertEquals(1, activeSections.length);
    assertEquals(section, activeSections[0].section);

    // Check that only the |active| section is visible.
    for (const s of page.shadowRoot.querySelectorAll('settings-section')) {
      assertEquals(s === activeSections[0], isVisible(s));
    }
  }

  test('OnlyOneSectionShown', async () => {
    assertTrue(loadTimeData.getBoolean('enableLandingPageRedesign'));
    const attribute =
        loadTimeData.getString('enableLandingPageRedesignAttribute');
    assertEquals('enable-landing-page-redesign', attribute);


    // Do this manually as it is normally part of settings.html, which is not
    // part of this test.
    document.documentElement.toggleAttribute(attribute, true);

    // Ensure that all settings-section instances are rendered.
    flush();
    await page.$$('#advancedPageTemplate').get();
    const sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);

    // RouteState.INITIAL -> RoutState.TOP_LEVEL
    // Check that only one is marked as |active|.
    assertActiveSection(routes.PEOPLE.section);
    assertTrue(!!page.shadowRoot.querySelector(
        'settings-section[active] settings-people-page'));

    // RouteState.TOP_LEVEL -> RoutState.SECTION
    // Check that navigating to a different route correctly updates the page.
    Router.getInstance().navigateTo(routes.SEARCH);
    await flushTasks();
    assertActiveSection(routes.SEARCH.section);
    assertTrue(!!page.shadowRoot.querySelector(
        'settings-section[active] settings-search-page'));

    // Helper functions.
    function getCardElement() {
      return page.shadowRoot.querySelector(
          'settings-section[active] settings-appearance-page');
    }

    function getDefault() {
      return getCardElement().shadowRoot.querySelector(
          'div[route-path="default"].iron-selected');
    }

    function getSubpage() {
      return getCardElement().shadowRoot.querySelector(
          'settings-subpage.iron-selected settings-appearance-fonts-page');
    }

    // RouteState.SECTION -> RoutState.SECTION
    Router.getInstance().navigateTo(routes.APPEARANCE);
    await flushTasks();
    assertActiveSection(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertTrue(!!getDefault());
    assertFalse(!!getSubpage());

    // RouteState.SECTION -> RoutState.SUBPAGE
    Router.getInstance().navigateTo(routes.FONTS);
    await flushTasks();
    assertActiveSection(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertFalse(!!getDefault());
    assertTrue(!!getSubpage());

    // RouteState.SUBPAGE -> RoutState.SECTION
    Router.getInstance().navigateTo(routes.APPEARANCE);
    await flushTasks();
    assertActiveSection(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertTrue(!!getDefault());
    assertFalse(!!getSubpage());

    // RouteState.SECTION -> RoutState.TOP_LEVEL
    Router.getInstance().navigateTo(routes.BASIC);
    await flushTasks();
    assertActiveSection(routes.PEOPLE.section);
  });
});
