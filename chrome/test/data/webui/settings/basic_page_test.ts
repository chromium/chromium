// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// clang-format off
import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {isChromeOS, isLacros, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, pageVisibility, PrivacyGuideBrowserProxy, PrivacyGuideBrowserProxyImpl, PrivacyGuideInteractions, Router, routes, SettingsBasicPageElement, SettingsIdleLoadElement, SettingsPrefsElement, SettingsSectionElement, StatusAction, SyncStatus} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, flushTasks, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on
class TestPrivacyGuideBrowserProxy extends TestBrowserProxy implements
    PrivacyGuideBrowserProxy {
  constructor() {
    super([
      'getPromoImpressionCount',
      'incrementPromoImpressionCount',
    ]);
  }

  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return 0;
  }

  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }
}

suite('SettingsBasicPage', () => {
  let page: SettingsBasicPageElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacyGuideEnabled: false,
    });
  });

  setup(async function() {
    document.body.innerHTML = '';

    // Because some test() cases below call navigateTo(), need to ensure that
    // the route is being reset before each test.
    Router.getInstance().navigateTo(routes.BASIC);

    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    page.scroller = document.body;

    // Need to wait for the 'show-container' event to fire after every
    // transition, to ensure no logic related to previous transitions is still
    // running when later transitions are tested.
    const whenDone = eventToPromise('show-container', page);

    // Ensure that all settings-section instances are rendered.
    flush();
    await page.shadowRoot!
        .querySelector<SettingsIdleLoadElement>('#advancedPageTemplate')!.get();
    const sections = page.shadowRoot!.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);

    await whenDone;
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('basic pages', function() {
    const sections = [
      'appearance', 'onStartup', 'people', 'search', 'autofill', 'safetyCheck',
      'privacy'
    ];
    if (!isChromeOS && !isLacros) {
      sections.push('defaultBrowser');
    }

    flush();

    for (const section of sections) {
      const sectionElement = page.shadowRoot!.querySelector(
          `settings-section[section=${section}]`);
      assertTrue(!!sectionElement, 'No sectionElement for section: ' + section);
    }
  });

  test('safetyCheckVisibilityTest', function() {
    // Set the visibility of the pages under test to "false".
    page.pageVisibility = Object.assign(pageVisibility || {}, {
      safetyCheck: false,
    });
    flush();

    const sectionElement =
        page.shadowRoot!.querySelector('settings-section-safety-check');
    assertFalse(!!sectionElement);
  });

  function assertActiveSection(section: string) {
    const activeSections =
        page.shadowRoot!.querySelectorAll<SettingsSectionElement>(
            'settings-section[active]');
    assertEquals(1, activeSections.length);
    assertEquals(section, activeSections[0]!.section);

    // Check that only the |active| section is visible.
    for (const s of page.shadowRoot!.querySelectorAll('settings-section')) {
      assertEquals(s === activeSections[0], isVisible(s));
    }
  }

  function assertActiveSubpage(section: string) {
    // Check that only the subpage of the |active| section is visible.
    const settingsPages = page.shadowRoot!.querySelectorAll(
        `settings-section[active] settings-${section}-page`);
    assertEquals(1, settingsPages.length);
    const subpages =
        settingsPages[0]!.shadowRoot!.querySelectorAll('settings-subpage');
    assertEquals(1, subpages.length);
    assertTrue(isVisible(subpages[0]!));
  }

  test('OnlyOneSectionShown', async () => {
    // RouteState.INITIAL -> RoutState.TOP_LEVEL
    // Check that only one is marked as |active|.
    assertActiveSection(routes.PEOPLE.section);
    assertTrue(!!page.shadowRoot!.querySelector(
        'settings-section[active] settings-people-page'));

    // RouteState.TOP_LEVEL -> RoutState.SECTION
    // Check that navigating to a different route correctly updates the page.
    let whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.SEARCH);
    await whenDone;
    await flushTasks();
    assertActiveSection(routes.SEARCH.section);
    assertTrue(!!page.shadowRoot!.querySelector(
        'settings-section[active] settings-search-page'));

    // Helper functions.
    function getCardElement() {
      return page.shadowRoot!.querySelector(
          'settings-section[active] settings-appearance-page');
    }

    function getDefault() {
      return getCardElement()!.shadowRoot!.querySelector(
          'div[route-path="default"].iron-selected');
    }

    function getSubpage() {
      return getCardElement()!.shadowRoot!.querySelector(
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

    // RouteState.TOP_LEVEL -> RoutState.SUBPAGE
    whenDone = eventToPromise('show-container', page);
    // Navigate specifically to a subpage that is *not* a child of
    // TOP_LEVEL_EQUIVALENT_ROUTE .
    Router.getInstance().navigateTo(routes.COOKIES);
    await whenDone;
    await flushTasks();
    assertActiveSubpage(routes.COOKIES.section);

    // RouteState.SUBPAGE -> RoutState.DIALOG when both reside under the same
    // SECTION.
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
    await flushTasks();

    // RouteState.DIALOG -> RoutState.SUBPAGE
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.SYNC);
    await whenDone;
    await flushTasks();
    assertActiveSubpage(routes.SYNC.section);

    // RouteState.SUBPAGE -> RoutState.DIALOG when they reside under different
    // sections.
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
    await whenDone;
    await flushTasks();
    assertPriacyActiveSections();
  });

  function assertPriacyActiveSections() {
    const activeSections =
        page.shadowRoot!.querySelectorAll<SettingsSectionElement>(
            'settings-section[active]');
    assertEquals(3, activeSections.length);
    // Privacy guide promo.
    assertEquals(
        routes.PRIVACY.section,
        activeSections[0]!.getAttribute('nest-under-section'));
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
    // Safety check.
    assertEquals(routes.SAFETY_CHECK.section, activeSections[1]!.section);
    assertEquals(
        routes.PRIVACY.section,
        activeSections[1]!.getAttribute('nest-under-section'));
    // Privacy section.
    assertEquals(routes.PRIVACY.section, activeSections[2]!.section);
  }

  // Test cases where a settings-section is appearing next to another section
  // using the |nest-under-section| attribute. Only one such case currently
  // exists.
  test('SometimesMoreSectionsShown', async () => {
    const whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.PRIVACY);
    await whenDone;
    await flushTasks();
    assertPriacyActiveSections();
  });
});

suite('PrivacyGuidePromo', () => {
  let page: SettingsBasicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let privacyGuideBrowserProxy: TestPrivacyGuideBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('privacyGuideEnabled'));
    privacyGuideBrowserProxy = new TestPrivacyGuideBrowserProxy();
    PrivacyGuideBrowserProxyImpl.setInstance(privacyGuideBrowserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-basic-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    page.scroller = document.body;
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    // Need to wait for the 'show-container' event to fire after every
    // transition, to ensure no logic related to previous transitions is still
    // running when later transitions are tested.
    const whenDone = eventToPromise('show-container', page);

    // Ensure that all settings-section instances are rendered.
    flush();
    await page.shadowRoot!
        .querySelector<SettingsIdleLoadElement>('#advancedPageTemplate')!.get();
    const sections = page.shadowRoot!.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);

    await whenDone;
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  // Same as the SometimesMoreSectionsShown test in the suite above, but
  // including the privacy guide.
  // TODO(crbug.com/1215630): Merge this test with the
  // SometimesMoreSectionsShown test when the privacy guide flag is removed.
  test('SometimesMoreSectionsShownWithPrivacyGuide', async () => {
    const whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.PRIVACY);
    await whenDone;
    await flushTasks();
    await privacyGuideBrowserProxy.whenCalled('incrementPromoImpressionCount');


    const activeSections =
        page.shadowRoot!.querySelectorAll<SettingsSectionElement>(
            'settings-section[active]');
    assertEquals(3, activeSections.length);
    // Privacy guide promo.
    assertEquals(
        routes.PRIVACY.section,
        activeSections[0]!.getAttribute('nest-under-section'));
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));
    // Safety check.
    assertEquals(routes.SAFETY_CHECK.section, activeSections[1]!.section);
    assertEquals(
        routes.PRIVACY.section,
        activeSections[1]!.getAttribute('nest-under-section'));
    // Privacy section.
    assertEquals(routes.PRIVACY.section, activeSections[2]!.section);
  });

  test('privacyGuidePromoVisibilityChildAccount', function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user signs in to a child user account. This hides the privacy guide
    // promo.
    let syncStatus:
        SyncStatus = {childUser: true, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer signed in to a child user account. This doesn't
    // show the promo.
    syncStatus = {childUser: false, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoVisibilityManaged', function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user becomes managed. This hides the privacy guide promo.
    webUIListenerCallback('is-managed-changed', true);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer managed. This doesn't show the promo.
    webUIListenerCallback('is-managed-changed', false);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoNoThanksTest', function() {
    // Make sure the pref is set and that privacy guide has never been seen
    // before.
    page.prefs.privacy_guide.viewed.value = false;
    flush();

    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // Click the no thanks button.
    const privacyGuidePromo =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePromo')!;
    privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
                                     '#noThanksButton')!.click();
    flush();

    // The privacy guide should be marked as seen and the promo no longer
    // visible.
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoStartMetrics', async function() {
    // Click the start button.
    const privacyGuidePromo =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePromo')!;
    privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
                                     '#startButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(result, PrivacyGuideInteractions.PROMO_ENTRY);
  });
});
