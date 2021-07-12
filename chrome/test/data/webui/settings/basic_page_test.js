// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// clang-format off
import 'chrome://settings/settings.js';

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData, pageVisibility, Router, routes} from 'chrome://settings/settings.js';

import {eventToPromise, flushTasks, isVisible} from '../test_util.m.js';
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

  suiteSetup(function() {
    assertTrue(loadTimeData.getBoolean('enableLandingPageRedesign'));
    const attribute =
        loadTimeData.getString('enableLandingPageRedesignAttribute');
    assertEquals('enable-landing-page-redesign', attribute);

    // Do this manually as it is normally part of settings.html, which is not
    // part of this test.
    document.documentElement.toggleAttribute(attribute, true);
  });

  setup(async function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    page.scroller = document.body;

    // Need to wait for the 'show-container' event to fire after every
    // transition, to ensure no logic related to previous transitions is still
    // running when later transitions are tested.
    const whenDone = eventToPromise('show-container', page);

    // Ensure that all settings-section instances are rendered.
    flush();
    await page.$$('#advancedPageTemplate').get();
    const sections = page.shadowRoot.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);

    await whenDone;
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

  /** @param {string} section */
  function assertActiveSubpage(section) {
    // Check that only the subpage of the |active| section is visible.
    const settingsPages = page.shadowRoot.querySelectorAll(
        `settings-section[active] settings-${section}-page`);
    assertEquals(1, settingsPages.length);
    const subpages =
        settingsPages[0].shadowRoot.querySelectorAll('settings-subpage');
    assertEquals(1, subpages.length);
    assertTrue(isVisible(subpages[0]));
  }

  test('OnlyOneSectionShown', async () => {
    // RouteState.INITIAL -> RoutState.TOP_LEVEL
    // Check that only one is marked as |active|.
    assertActiveSection(routes.PEOPLE.section);
    assertTrue(!!page.shadowRoot.querySelector(
        'settings-section[active] settings-people-page'));

    // RouteState.TOP_LEVEL -> RoutState.SECTION
    // Check that navigating to a different route correctly updates the page.
    let whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.SEARCH);
    await whenDone;
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
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.APPEARANCE);
    await whenDone;
    await flushTasks();
    assertActiveSection(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertTrue(!!getDefault());
    assertFalse(!!getSubpage());

    // RouteState.SECTION -> RoutState.SUBPAGE
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.FONTS);
    await whenDone;
    await flushTasks();
    assertActiveSubpage(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertFalse(!!getDefault());
    assertTrue(!!getSubpage());

    // RouteState.SUBPAGE -> RoutState.SECTION
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.APPEARANCE);
    await whenDone;
    await flushTasks();
    assertActiveSection(routes.APPEARANCE.section);
    assertTrue(!!getCardElement());
    assertTrue(!!getDefault());
    assertFalse(!!getSubpage());

    // RouteState.SECTION -> RoutState.TOP_LEVEL
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.BASIC);
    await whenDone;
    await flushTasks();
    assertActiveSection(routes.PEOPLE.section);
  });

  // Test cases where a settings-section is appearing next to another section
  // using the |nest-under-section| attribute. Only one such case currently
  // exists.
  test('SometimesMoreSectionsShown', async () => {
    const whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.PRIVACY);
    await whenDone;
    await flushTasks();

    const activeSections =
        page.shadowRoot.querySelectorAll('settings-section[active]');
    assertEquals(2, activeSections.length);
    assertEquals(routes.SAFETY_CHECK.section, activeSections[0].section);
    assertEquals(
        routes.PRIVACY.section,
        activeSections[0].getAttribute('nest-under-section'));
    assertEquals(routes.PRIVACY.section, activeSections[1].section);
  });
});
