// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// clang-format off
import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {isChromeOS, isLacros} from 'chrome://resources/js/platform.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsBasicPageElement, SettingsIdleLoadElement, SettingsPrefsElement, SettingsSectionElement, SyncStatus} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, pageVisibility, PerformanceBrowserProxyImpl, PrivacyGuideBrowserProxyImpl, PrivacyGuideInteractions, resetRouterForTesting, Router, routes, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPrivacyGuideBrowserProxy} from './test_privacy_guide_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';

// clang-format on
suite('BasicPage', () => {
  let page: SettingsBasicPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    // Don't show the privacy guide promo in regular tests.
    loadTimeData.overrideValues({showPrivacyGuide: false});
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertFalse(loadTimeData.getBoolean('showPrivacyGuide'));

    // Because some test() cases below call navigateTo(), need to ensure that
    // the route is being reset before each test.
    Router.getInstance().navigateTo(routes.BASIC);

    page = document.createElement('settings-basic-page');
    page.prefs = settingsPrefs.prefs!;
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
      'appearance',
      'onStartup',
      'people',
      'search',
      'autofill',
      'privacy',
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

  // TODO(crbug.com/40277421): Remove after SafetyHub launched.
  test('safetyCheckVisibilityTest', function() {
    function querySafetyCheckSection() {
      return page.shadowRoot!.querySelector('#safetyCheckSettingsSection');
    }

    // Set the visibility of the pages under test to their default value.
    page.pageVisibility = pageVisibility || {};
    flush();

    // When enabled, SafetyHub replaces SafetyCheck by default.
    assertFalse(
        !!querySafetyCheckSection(),
        'SafetyCheck should not be visible with default page visibility');
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
          'div[route-path="default"].selected');
    }

    function getSubpage() {
      return getCardElement()!.shadowRoot!.querySelector(
          'settings-subpage.selected settings-appearance-fonts-page');
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

    // RouteState.SUBPAGE -> RoutState.SUBPAGE when they reside under different
    // sections.
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.FONTS);
    await whenDone;
    await flushTasks();
    assertActiveSubpage(routes.APPEARANCE.section);

    // RouteState.SUBPAGE -> RoutState.DIALOG when they reside under different
    // sections.
    whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
    await whenDone;
    await flushTasks();
    assertPrivacyActiveSections();
  });

  function assertPrivacyActiveSections() {
    const activeSections =
        page.shadowRoot!.querySelectorAll<SettingsSectionElement>(
            'settings-section[active]');
    assertEquals(3, activeSections.length);
    // Privacy guide promo.
    assertEquals(
        routes.PRIVACY.section,
        activeSections[0]!.getAttribute('nest-under-section'));
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
    // Safety Hub.
    assertEquals('safetyHubEntryPoint', activeSections[1]!.section);
    assertEquals(
        routes.PRIVACY.section,
        activeSections[1]!.getAttribute('nest-under-section'));
    assertTrue(isChildVisible(page, '#safetyHubEntryPointSection'));
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
    assertPrivacyActiveSections();
  });
});

// TODO(crbug.com/40184479): Remove once the privacy guide promo has been
// removed.
suite('PrivacyGuidePromo', () => {
  let page: SettingsBasicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let privacyGuideBrowserProxy: TestPrivacyGuideBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({showPrivacyGuide: true});
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    privacyGuideBrowserProxy = new TestPrivacyGuideBrowserProxy();
    PrivacyGuideBrowserProxyImpl.setInstance(privacyGuideBrowserProxy);
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = document.createElement('settings-basic-page');
    page.prefs = settingsPrefs.prefs!;
    // The promo is only shown when privacy guide hasn't been visited yet.
    page.setPrefValue('privacy_guide.viewed', false);
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

  // Same as the SometimesMoreSectionsShown test in the suite above, but
  // including the privacy guide.
  test('SometimesMoreSectionsShownWithPrivacyGuide', async function() {
    const whenDone = eventToPromise('show-container', page);
    Router.getInstance().navigateTo(routes.PRIVACY);
    await whenDone;
    await microtasksFinished();
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
    // Safety Hub entry point.
    assertEquals('safetyHubEntryPoint', activeSections[1]!.section);
    assertEquals(
        routes.PRIVACY.section,
        activeSections[1]!.getAttribute('nest-under-section'));
    // Privacy section.
    assertEquals(routes.PRIVACY.section, activeSections[2]!.section);
  });

  test('privacyGuidePromoVisibilitySupervisedAccount', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user signs in to a supervised user account. This hides the privacy
    // guide promo.
    let syncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', syncStatus);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer signed in to a supervised user account. This
    // doesn't show the promo.
    syncStatus = {supervisedUser: false, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoVisibilityManaged', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // The user becomes managed. This hides the privacy guide promo.
    webUIListenerCallback('is-managed-changed', true);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));

    // The user is no longer managed. This doesn't show the promo.
    webUIListenerCallback('is-managed-changed', false);
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoNoThanksTest', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));
    assertFalse(page.getPref('privacy_guide.viewed').value);

    // Click the no thanks button.
    const privacyGuidePromo =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePromo');
    assertTrue(!!privacyGuidePromo);
    const noThanksButton =
        privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
            '#noThanksButton');
    assertTrue(!!noThanksButton);
    noThanksButton.click();
    await microtasksFinished();

    // The privacy guide should be marked as seen and the promo no longer
    // visible.
    assertFalse(isChildVisible(page, '#privacyGuidePromo'));
  });

  test('privacyGuidePromoStartMetrics', async function() {
    assertTrue(isChildVisible(page, '#privacyGuidePromo'));

    // Click the start button.
    const privacyGuidePromo =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePromo');
    assertTrue(!!privacyGuidePromo);
    const startButton =
        privacyGuidePromo.shadowRoot!.querySelector<HTMLElement>(
            '#startButton');
    assertTrue(!!startButton);
    startButton.click();
    await microtasksFinished();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(result, PrivacyGuideInteractions.PROMO_ENTRY);
  });
});

suite('Performance', () => {
  let page: SettingsBasicPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;

  function queryPerformanceSettingsSection(): SettingsSectionElement|null {
    return page.shadowRoot!.querySelector('#performanceSettingsSection');
  }

  function queryMemorySettingsSection(): SettingsSectionElement|null {
    return page.shadowRoot!.querySelector('#memorySettingsSection');
  }

  function queryBatterySettingsSection(): SettingsSectionElement|null {
    return page.shadowRoot!.querySelector('#batterySettingsSection');
  }

  function querySpeedSettingsSection(): SettingsSectionElement|null {
    return page.shadowRoot!.querySelector('#speedSettingsSection');
  }

  async function createNewBasicPage() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    flush();
    await page.shadowRoot!
        .querySelector<SettingsIdleLoadElement>('#advancedPageTemplate')!.get();
    const sections = page.shadowRoot!.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);
  }

  test('performanceSectionTitlesVisible', async function() {
    await createNewBasicPage();
    page.pageVisibility = pageVisibility || {};
    flush();

    assertEquals(
        queryPerformanceSettingsSection()!.shadowRoot!.querySelector('h2')
            ?.innerText,
        loadTimeData.getString('generalPageTitle'));
    assertEquals(
        queryMemorySettingsSection()!.shadowRoot!.querySelector('h2')
            ?.innerText,
        loadTimeData.getString('memoryPageTitle'));
    assertEquals(
        queryBatterySettingsSection()!.shadowRoot!.querySelector('h2')
            ?.innerText,
        loadTimeData.getString('batteryPageTitle'));
    assertEquals(
        querySpeedSettingsSection()!.shadowRoot!.querySelector('h2')?.innerText,
        loadTimeData.getString('speedPageTitle'));
  });

  test('performanceVisibilityTestFeaturesAvailable', async function() {
    await createNewBasicPage();
    // Set the visibility of the pages under test to their default value.
    page.pageVisibility = pageVisibility || {};
    flush();

    assertTrue(
        !!queryPerformanceSettingsSection(),
        'Performance section should exist with default page visibility');
    assertTrue(
        !!queryMemorySettingsSection(),
        'Memory section should exist with default page visibility');
    assertTrue(
        !!queryBatterySettingsSection(),
        'Battery section should exist with default page visibility');
    assertTrue(
        !!querySpeedSettingsSection(),
        'Speed section should exist with default page visibility');

    // Set the visibility of the pages under test to "false".
    page.pageVisibility = Object.assign(pageVisibility || {}, {
      performance: false,
    });
    flush();

    assertFalse(
        !!queryPerformanceSettingsSection(),
        'Performance section should not exist when visibility is false');
    assertFalse(
        !!queryMemorySettingsSection(),
        'Memory section should not exist when visibility is false');
    assertFalse(
        !!queryBatterySettingsSection(),
        'Battery section should not exist when visibility is false');
    assertFalse(
        !!querySpeedSettingsSection(),
        'Speed section should not exist when visibility is false');
  });

  test('performanceVisibilityTestDeviceHasBattery', async function() {
    await createNewBasicPage();
    page.pageVisibility = pageVisibility || {};
    flush();

    await performanceBrowserProxy.whenCalled('getDeviceHasBattery');
    const batterySettingsSection = queryBatterySettingsSection();
    assertTrue(!!batterySettingsSection);
    assertTrue(
        batterySettingsSection.hidden,
        'Battery section should be hidden at by default');

    // Simulate OnDeviceHasBatteryChanged from backend
    webUIListenerCallback('device-has-battery-changed', true);
    assertFalse(
        batterySettingsSection.hidden,
        'Battery section should be visible after being notified that the ' +
            'device has a battery');
  });
});

// TODO(crbug.com/40277421): Remove after SafetyHub launched.
suite('SafetyHubDisabled', () => {
  let page: SettingsBasicPageElement;

  setup(async function() {
    loadTimeData.overrideValues({enableSafetyHub: false});
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    flush();
    await page.shadowRoot!
        .querySelector<SettingsIdleLoadElement>('#advancedPageTemplate')!.get();
    const sections = page.shadowRoot!.querySelectorAll('settings-section');
    assertTrue(sections.length > 1);
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });


  test('safety check visible', function() {
    function querySafetyCheckSection() {
      return page.shadowRoot!.querySelector('#safetyCheckSettingsSection');
    }

    // Set the visibility of the pages under test to their default value.
    page.pageVisibility = pageVisibility || {};
    flush();

    assertTrue(
        !!querySafetyCheckSection(),
        'Safety check section should be visible with default page visibility');

    // Set the visibility of the pages under test to "false".
    page.pageVisibility = Object.assign(pageVisibility || {}, {
      safetyCheck: false,
    });
    flush();

    assertFalse(!!querySafetyCheckSection());
  });

  test('safety hub not visible', function() {
    function querySafetyHubSection() {
      return page.shadowRoot!.querySelector('#safetyHubEntryPointSection');
    }

    // Set the visibility of the pages under test to their default value.
    page.pageVisibility = pageVisibility || {};
    flush();

    assertFalse(
        !!querySafetyHubSection(),
        'Safety Hub section should not be visible with default visibility');
  });
});

suite('ExperimentalAdvanced', () => {
  let page: SettingsBasicPageElement;

  function createBasicPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
    flush();
  }

  test('sectionNotVisible', function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: false});
    resetRouterForTesting();

    createBasicPage();
    const sectionElement =
        page.shadowRoot!.querySelector('settings-section[section=ai]');
    assertFalse(!!sectionElement);
  });

  test('sectionVisible', function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: true});
    resetRouterForTesting();

    createBasicPage();
    const sectionElement =
        page.shadowRoot!.querySelector('settings-section[section=ai]');
    assertTrue(!!sectionElement);
  });

  test('infoCardNotVisible', function() {
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      enableAiSettingsPageRefresh: false,
    });
    resetRouterForTesting();

    createBasicPage();
    const sectionElement =
        page.shadowRoot!.querySelector('settings-section[section=aiInfoCard]');
    assertFalse(!!sectionElement);
  });

  test('infoCardVisible', function() {
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      enableAiSettingsPageRefresh: true,
    });
    resetRouterForTesting();

    createBasicPage();
    const sectionElement =
        page.shadowRoot!.querySelector<SettingsSectionElement>(
            'settings-section[section=aiInfoCard]');
    assertTrue(!!sectionElement);
    assertEquals(
        routes.AI.section, sectionElement!.getAttribute('nest-under-section'));
  });
});
