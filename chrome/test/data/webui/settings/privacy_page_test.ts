// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ClearBrowsingDataBrowserProxyImpl, ContentSettingsTypes, CookiePrimarySetting, SafeBrowsingSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement, HatsBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyPageBrowserProxyImpl, Route, Router, routes, SecureDnsMode, SettingsPrivacyPageElement, StatusAction, SyncStatus, TrustSafetyInteraction} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

const redesignedPages: Route[] = [
  routes.SITE_SETTINGS_ADS,
  routes.SITE_SETTINGS_AR,
  routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
  routes.SITE_SETTINGS_BACKGROUND_SYNC,
  routes.SITE_SETTINGS_CAMERA,
  routes.SITE_SETTINGS_CLIPBOARD,
  routes.SITE_SETTINGS_FEDERATED_IDENTITY_API,
  routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
  routes.SITE_SETTINGS_HANDLERS,
  routes.SITE_SETTINGS_HID_DEVICES,
  routes.SITE_SETTINGS_IDLE_DETECTION,
  routes.SITE_SETTINGS_IMAGES,
  routes.SITE_SETTINGS_JAVASCRIPT,
  routes.SITE_SETTINGS_LOCAL_FONTS,
  routes.SITE_SETTINGS_LOCATION,
  routes.SITE_SETTINGS_MICROPHONE,
  routes.SITE_SETTINGS_MIDI_DEVICES,
  routes.SITE_SETTINGS_NOTIFICATIONS,
  routes.SITE_SETTINGS_PAYMENT_HANDLER,
  routes.SITE_SETTINGS_PDF_DOCUMENTS,
  routes.SITE_SETTINGS_POPUPS,
  routes.SITE_SETTINGS_PROTECTED_CONTENT,
  routes.SITE_SETTINGS_SENSORS,
  routes.SITE_SETTINGS_SERIAL_PORTS,
  routes.SITE_SETTINGS_SOUND,
  routes.SITE_SETTINGS_USB_DEVICES,
  routes.SITE_SETTINGS_VR,

  // TODO(crbug.com/1128902) After restructure add coverage for elements on
  // routes which depend on flags being enabled.
  // routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
  // routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
  // routes.SITE_SETTINGS_WINDOW_PLACEMENT,

  // Doesn't contain toggle or radio buttons
  // routes.SITE_SETTINGS_INSECURE_CONTENT,
  // routes.SITE_SETTINGS_ZOOM_LEVELS,
];

suite('PrivacyPage', function() {
  let page: SettingsPrivacyPageElement;
  let testClearBrowsingDataBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  const testLabels: string[] = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      privacyGuideEnabled: false,
    });
  });

  setup(function() {
    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(
        testClearBrowsingDataBrowserProxy);
    const testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]!);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    page.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
      },
      safebrowsing: {
        enabled: {value: true},
        scout_reporting_enabled: {value: true},
        enhanced: {value: false}
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('showClearBrowsingDataDialog', function() {
    assertFalse(!!page.shadowRoot!.querySelector(
        'settings-clear-browsing-data-dialog'));
    page.$.clearBrowsingData.click();
    flush();

    const dialog =
        page.shadowRoot!.querySelector('settings-clear-browsing-data-dialog');
    assertTrue(!!dialog);
  });

  test('CookiesLinkRowSublabel', async function() {
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    assertEquals(page.$.cookiesLinkRow.subLabel, testLabels[0]);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(page.$.cookiesLinkRow.subLabel, testLabels[1]);
  });

  test('privacyGuideRowNotVisible', function() {
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('ContentSettingsVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    await flushTasks();

    // All redesigned pages, except notifications, protocol handlers, pdf
    // documents and protected content (except chromeos and win), will use a
    // settings-category-default-radio-group.
    // <if expr="chromeos_ash or chromeos_lacros or is_win">
    assertEquals(
        page.shadowRoot!
            .querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 3);
    // </if>
    // <if expr="not chromeos_ash and not chromeos_lacros and not is_win">
    assertEquals(
        page.shadowRoot!
            .querySelectorAll('settings-category-default-radio-group')
            .length,
        redesignedPages.length - 4);
    // </if>
  });

  test('NotificationPage', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    await flushTasks();

    assertTrue(isChildVisible(page, '#notificationRadioGroup'));
    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions')!;
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
    assertFalse(isChildVisible(page, 'category-default-setting'));
  });

  test('privacySandboxRestricted', function() {
    assertFalse(isChildVisible(page, '#privacySandboxLinkRow'));
  });
});

suite('PrivacySandboxEnabled', function() {
  let page: SettingsPrivacyPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
      privacySandboxSettings3Enabled: false,
    });
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    page.prefs = {
      privacy_sandbox: {
        apis_enabled: {value: true},
        apis_enabled_v2: {value: true},
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacySandboxRestricted', function() {
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
  });

  test('privacySandboxRowSublabel', async function() {
    page.set('prefs.privacy_sandbox.apis_enabled.value', true);
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    assertTrue(isChildVisible(page, '#privacySandboxLinkRow'));
    const privacySandboxLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>(
            '#privacySandboxLinkRow')!;
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsEnabled'),
        privacySandboxLinkRow.subLabel);

    page.set('prefs.privacy_sandbox.apis_enabled.value', false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsDisabled'),
        privacySandboxLinkRow.subLabel);

    loadTimeData.overrideValues({privacySandboxSettings3Enabled: true});
    page.set('prefs.privacy_sandbox.apis_enabled.value', true);
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsDisabled'),
        privacySandboxLinkRow.subLabel);

    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('privacySandboxTrialsEnabled'),
        privacySandboxLinkRow.subLabel);
  });

  test('clickPrivacySandboxRow', async function() {
    page.shadowRoot!.querySelector<CrLinkRowElement>(
                        '#privacySandboxLinkRow')!.click();
    // Ensure UMA is logged.
    assertEquals(
        'Settings.PrivacySandbox.OpenedFromSettingsParent',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });
});

suite('PrivacyGuideEnabled', function() {
  let page: SettingsPrivacyPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    page.prefs = {
      // Need privacy_sandbox pref for the page's setup.
      privacy_sandbox: {
        apis_enabled: {value: true},
        apis_enabled_v2: {value: true},
      },
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacyGuideRowVisibleChildAccount', function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user signs in to a child user account. This hides the privacy guide
    // entry point.
    const syncStatus:
        SyncStatus = {childUser: true, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer signed in to a child user account. This doesn't
    // show the entry point.
    syncStatus.childUser = false;
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowVisibleManaged', function() {
    assertTrue(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user becomes managed. This hides the privacy guide entry point.
    webUIListenerCallback('is-managed-changed', true);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));

    // The user is no longer managed. This doesn't show the entry point.
    webUIListenerCallback('is-managed-changed', false);
    flush();
    assertFalse(isChildVisible(page, '#privacyGuideLinkRow'));
  });

  test('privacyGuideRowClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#privacyGuideLinkRow')!.click();

    const result = await metricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY, result);

    // Ensure the correct route has been navigated to.
    assertEquals(routes.PRIVACY_GUIDE, Router.getInstance().getCurrentRoute());

    // Ensure the privacy guide dialog is shown.
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePage'));
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#privacyGuideDialog'));
  });
});

// TODO(1215630): Remove once #privacy-guide-2 has been rolled out.
suite('PrivacyGuide2Disabled', function() {
  let page: SettingsPrivacyPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacyGuide2Enabled: false,
    });
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    page.prefs = {
      // Need privacy_sandbox pref for the page's setup.
      privacy_sandbox: {
        apis_enabled: {value: true},
        apis_enabled_v2: {value: true},
      },
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  test('privacyGuideRowClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#privacyGuideLinkRow')!.click();

    const result = await metricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY, result);

    // Ensure the correct route has been navigated to.
    assertEquals(routes.PRIVACY_GUIDE, Router.getInstance().getCurrentRoute());

    // Ensure the privacy guide page is shown.
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#privacyGuidePage'));
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#privacyGuideDialog'));
  });
});

suite('PrivacyPageSound', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsPrivacyPageElement;

  function getToggleElement() {
    return page.shadowRoot!.querySelector<HTMLElement>(
        '#block-autoplay-setting')!;
  }

  setup(() => {
    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SOUND);
    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
  });

  test('UpdateStatus', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushTasks().then(() => {
      // Check that we are on and enabled.
      assertFalse(getToggleElement().hasAttribute('disabled'));
      assertTrue(getToggleElement().hasAttribute('checked'));

      // Toggle the pref off.
      webUIListenerCallback(
          'onBlockAutoplayStatusChanged',
          {pref: {value: false}, enabled: true});

      return flushTasks().then(() => {
        // Check that we are off and enabled.
        assertFalse(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        // Disable the autoplay status toggle.
        webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: false}, enabled: false});

        return flushTasks().then(() => {
          // Check that we are off and disabled.
          assertTrue(getToggleElement().hasAttribute('disabled'));
          assertFalse(getToggleElement().hasAttribute('checked'));
        });
      });
    });
  });

  test('Hidden', () => {
    assertTrue(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertFalse(getToggleElement().hidden);

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

    page.remove();
    page = document.createElement('settings-privacy-page');
    document.body.appendChild(page);

    return flushTasks().then(() => {
      assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
      assertTrue(getToggleElement().hidden);
    });
  });

  test('Click', () => {
    assertTrue(getToggleElement().hasAttribute('disabled'));
    assertFalse(getToggleElement().hasAttribute('checked'));

    webUIListenerCallback(
        'onBlockAutoplayStatusChanged', {pref: {value: true}, enabled: true});

    return flushTasks().then(() => {
      // Check that we are on and enabled.
      assertFalse(getToggleElement().hasAttribute('disabled'));
      assertTrue(getToggleElement().hasAttribute('checked'));

      // Click on the toggle and wait for the proxy to be called.
      getToggleElement().click();
      return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
          .then((enabled) => {
            assertFalse(enabled);
          });
    });
  });
});

suite('HappinessTrackingSurveys', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let page: SettingsPrivacyPageElement;

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-page');
    // Initialize the privacy page pref. Security page manually expands
    // the initially selected safe browsing option so the pref object
    // needs to be defined.
    page.prefs = {
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        cookie_session_only: {value: false},
        cookie_primary_setting:
            {type: chrome.settingsPrivate.PrefType.NUMBER, value: 0},
        password_manager_leak_detection: {value: false},
      },
      profile: {password_manager_leak_detection: {value: false}},
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ClearBrowsingDataTrigger', async function() {
    page.$.clearBrowsingData.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('CookiesTrigger', async function() {
    page.$.cookiesLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('SecurityTrigger', async function() {
    page.$.securityLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });

  test('PermissionsTrigger', async function() {
    page.$.permissionsLinkRow.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.USED_PRIVACY_CARD, interaction);
  });
});
