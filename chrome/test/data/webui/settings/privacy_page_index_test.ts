// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {Route, SettingsPrivacyPageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetPageVisibilityForTesting, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

interface RouteInfo {
  route: Route;
  viewId: string;
  parentViewId?: string;
}

suite('PrivacyPageIndex', function() {
  let index: SettingsPrivacyPageIndexElement;

  async function createPrivacyPageIndex(overrides?: {[key: string]: any}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues(Object.assign(
        {
          enableBundledSecuritySettings: false,
          enableCapturedSurfaceControl: false,
          enableExperimentalWebPlatformFeatures: false,
          enableFederatedIdentityApiContentSetting: false,
          enableHandTrackingContentSetting: false,
          enableKeyboardLockPrompt: false,
          enableLocalNetworkAccessSetting: false,
          enablePaymentHandlerContentSetting: false,
          enablePersistentPermissions: false,
          enableSafeBrowsingSubresourceFilter: false,
          enableSecurityKeysSubpage: false,
          // <if expr="is_chromeos">
          enableSmartCardReadersContentSetting: false,
          // </if>
          enableWebAppInstallation: false,
          enableWebBluetoothNewPermissionsBackend: false,
          enableWebPrintingContentSetting: false,
          isGuest: false,
          isPrivacySandboxRestricted: false,
          isPrivacySandboxRestrictedNoticeEnabled: false,
        },
        overrides || {}));
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    SiteSettingsBrowserProxyImpl.setInstance(
        new TestSiteSettingsBrowserProxy());

    index = document.createElement('settings-privacy-page-index');
    index.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.BASIC);
    document.body.appendChild(index);
    return flushTasks();
  }

  async function testViewsForRoute(
      route: Route, viewIds: string[], parentViewId: string|null = null) {
    Router.getInstance().navigateTo(route);
    await flushTasks();
    await waitBeforeNextRender(index);

    for (const id of viewIds) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`),
          `Failed for route '${route.path}'`);

      if (parentViewId) {
        const view = index.$.viewManager.querySelector(
            `#${id}[slot=view][data-parent-view-id=${parentViewId}]`);
        assertTrue(!!view);
        assertEquals(route.path, view.getAttribute('route-path'));
      } else {
        assertTrue(!!index.$.viewManager.querySelector(
            `#${id}[slot=view]:not([data-parent-view-id])`));
      }
    }
  }

  setup(function() {
    return createPrivacyPageIndex();
  });

  suite('Main', function() {
    test('Routing', async function() {
      const defaultViews =
          ['privacy', 'privacyGuidePromo', 'safetyHubEntryPoint'];

      await testViewsForRoute(routes.PRIVACY, defaultViews);
      await testViewsForRoute(routes.BASIC, defaultViews);

      // Non-exhaustive list of PRIVACY child routes to check.
      const routesToVisit: RouteInfo[] = [
        {route: routes.CLEAR_BROWSER_DATA, viewId: 'privacy'},
        {route: routes.COOKIES, viewId: 'cookies', parentViewId: 'privacy'},
        {
          route: routes.SAFETY_HUB,
          viewId: 'safetyHub',
          parentViewId: 'safetyHubEntryPoint',
        },
        {
          route: routes.SECURITY,
          viewId: 'security',
          parentViewId: 'privacy',
        },
      ];

      for (const routeInfo of routesToVisit) {
        await testViewsForRoute(
            routeInfo.route, [routeInfo.viewId], routeInfo.parentViewId);
      }
    });

    test('RoutingLazyRender', async function() {
      assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
      await flushTasks();
      await waitBeforeNextRender(index);
      assertFalse(!!index.$.viewManager.querySelector('#privacy'));
      await testViewsForRoute(routes.PRIVACY, ['privacy']);
    });

    test('RoutingPrivacySandboxRestrictedFalse', async function() {
      await createPrivacyPageIndex({
        isPrivacySandboxRestricted: false,
        isPrivacySandboxRestrictedNoticeEnabled: false,
      });

      // Necessary for the PRIVACY_SANDBOX_MANAGE_TOPICS route to not
      // automatically redirect to its parent.
      index.setPrefValue('privacy_sandbox.m1.topics_enabled', true);

      const routesToVisit: RouteInfo[] = [
        {
          route: routes.PRIVACY_SANDBOX,
          viewId: 'privacySandbox',
          parentViewId: 'privacy',
        },
        {
          route: routes.PRIVACY_SANDBOX_TOPICS,
          viewId: 'privacySandboxTopics',
          parentViewId: 'privacy',
        },
        {
          route: routes.PRIVACY_SANDBOX_MANAGE_TOPICS,
          viewId: 'privacySandboxManageTopics',
          parentViewId: 'privacy',
        },
        {
          route: routes.PRIVACY_SANDBOX_FLEDGE,
          viewId: 'privacySandboxFledge',
          parentViewId: 'privacy',
        },
        {
          route: routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
          viewId: 'privacySandboxAdMeasurement',
          parentViewId: 'privacy',
        },
      ];

      for (const routeInfo of routesToVisit) {
        await testViewsForRoute(
            routeInfo.route, [routeInfo.viewId], routeInfo.parentViewId);
      }
    });

    test('RoutingPrivacySandboxRestrictedNoticeEnableTrue', async function() {
      await createPrivacyPageIndex({
        isPrivacySandboxRestricted: true,
        isPrivacySandboxRestrictedNoticeEnabled: true,
      });

      // Necessary for the PRIVACY_SANDBOX_MANAGE_TOPICS route to not
      // automatically redirect to its parent.
      index.setPrefValue('privacy_sandbox.m1.topics_enabled', true);

      const routesToVisit: RouteInfo[] = [
        {
          route: routes.PRIVACY_SANDBOX,
          viewId: 'privacySandbox',
          parentViewId: 'privacy',
        },
        {
          route: routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
          viewId: 'privacySandboxAdMeasurement',
          parentViewId: 'privacy',
        },
      ];

      for (const routeInfo of routesToVisit) {
        await testViewsForRoute(
            routeInfo.route, [routeInfo.viewId], routeInfo.parentViewId);
      }
    });

    // TODO(crbug.com/417690232): Delete once kBundledSecuritySettings is
    // launched.
    test('RoutingSecurityV2', async function() {
      assertFalse(loadTimeData.getBoolean('enableBundledSecuritySettings'));

      // Case where old UI should exist.
      await createPrivacyPageIndex();
      await testViewsForRoute(routes.SECURITY, ['security'], 'privacy');
      assertTrue(!!index.shadowRoot!.querySelector('settings-security-page'));
      assertFalse(
          !!index.shadowRoot!.querySelector('settings-security-page-v2'));

      // Case where new UI should exist.
      await createPrivacyPageIndex({enableBundledSecuritySettings: true});
      await testViewsForRoute(routes.SECURITY, ['security'], 'privacy');
      assertFalse(!!index.shadowRoot!.querySelector('settings-security-page'));
      assertTrue(
          !!index.shadowRoot!.querySelector('settings-security-page-v2'));
    });

    test('RoutingSecurityKeys', async function() {
      assertFalse(loadTimeData.getBoolean('enableSecurityKeysSubpage'));
      await createPrivacyPageIndex({enableSecurityKeysSubpage: true});
      return testViewsForRoute(
          routes.SECURITY_KEYS, ['securityKeys'], 'privacy');
    });

    // <if expr="is_chromeos">
    test('RoutingGuestMode', async function() {
      assertFalse(loadTimeData.getBoolean('isGuest'));
      assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
      await createPrivacyPageIndex({isGuest: true});
      assertTrue(
          !!index.$.viewManager.querySelector('#privacy.active[slot=view]'));
    });
    // </if>

    // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
    // inherited correctly.
    test('Search', async function() {
      index.inSearchMode = true;
      await flushTasks();

      // Case1: Results within the "Privacy and security" card.
      let result = await index.searchContents('Privacy and security');
      assertFalse(result.canceled);
      assertTrue(result.matchCount > 0);
      assertFalse(result.wasClearSearch);

      // Case2: Results within the "Safety check" card.
      result = await index.searchContents('Safety check');
      assertFalse(result.canceled);
      assertTrue(result.matchCount > 0);
      assertFalse(result.wasClearSearch);
    });
  });

  // Site settings tests are placed on a dedicated suite() to reduce the chances
  // of timeouts on dbg bots.
  suite('SiteSettings', function() {
    test('Routing', async function() {
      // SITE_SETTINGS and child routes to check.
      const routesToVisit: RouteInfo[] = [
        {
          route: routes.SITE_SETTINGS,
          viewId: 'siteSettings',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_ALL,
          viewId: 'siteSettingsAll',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_AR,
          viewId: 'siteSettingsAr',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_AUTO_VERIFY,
          viewId: 'siteSettingsAutoVerify',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
          viewId: 'siteSettingsAutomaticDownloads',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_AUTOMATIC_FULLSCREEN,
          viewId: 'siteSettingsAutomaticFullscreen',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_BACKGROUND_SYNC,
          viewId: 'siteSettingsBackgroundSync',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_CAMERA,
          viewId: 'siteSettingsCamera',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_CLIPBOARD,
          viewId: 'siteSettingsClipboard',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
          viewId: 'siteSettingsFilesystemWrite',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_IDLE_DETECTION,
          viewId: 'siteSettingsIdleDetection',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_IMAGES,
          viewId: 'siteSettingsImages',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER,
          viewId: 'siteSettingsJavascriptOptimizer',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_HANDLERS,
          viewId: 'siteSettingsHandlers',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_HID_DEVICES,
          viewId: 'siteSettingsHidDevices',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_JAVASCRIPT,
          viewId: 'siteSettingsJavascript',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_LOCAL_FONTS,
          viewId: 'siteSettingsLocalFonts',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_LOCATION,
          viewId: 'siteSettingsLocation',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_MICROPHONE,
          viewId: 'siteSettingsMicrophone',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_MIDI_DEVICES,
          viewId: 'siteSettingsMidiDevices',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_MIXEDSCRIPT,
          viewId: 'siteSettingsMixedscript',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_NOTIFICATIONS,
          viewId: 'siteSettingsNotifications',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_PDF_DOCUMENTS,
          viewId: 'siteSettingsPdfDocuments',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_POPUPS,
          viewId: 'siteSettingsPopups',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_PROTECTED_CONTENT,
          viewId: 'siteSettingsProtectedContent',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_SENSORS,
          viewId: 'siteSettingsSensors',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_SERIAL_PORTS,
          viewId: 'siteSettingsSerialPorts',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_SITE_DATA,
          viewId: 'siteSettingsSiteData',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_SITE_DETAILS,
          viewId: 'siteSettingsSiteDetails',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_SOUND,
          viewId: 'siteSettingsSound',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_STORAGE_ACCESS,
          viewId: 'siteSettingsStorageAccess',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_USB_DEVICES,
          viewId: 'siteSettingsUsbDevices',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_VR,
          viewId: 'siteSettingsVr',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_WINDOW_MANAGEMENT,
          viewId: 'siteSettingsWindowManagement',
          parentViewId: 'privacy',
        },
        {
          route: routes.SITE_SETTINGS_ZOOM_LEVELS,
          viewId: 'siteSettingsZoomLevels',
          parentViewId: 'privacy',
        },
      ];

      for (const routeInfo of routesToVisit) {
        await testViewsForRoute(
            routeInfo.route, [routeInfo.viewId], routeInfo.parentViewId);
      }
    });

    test('RoutingAds', async function() {
      assertFalse(
          loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'));
      await createPrivacyPageIndex({enableSafeBrowsingSubresourceFilter: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_ADS, ['siteSettingsAds'], 'privacy');
    });

    test('RoutingBluetoothDevices', async function() {
      assertFalse(
          loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'));
      await createPrivacyPageIndex(
          {enableWebBluetoothNewPermissionsBackend: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
          ['siteSettingsBluetoothDevices'], 'privacy');
    });

    test('RoutingBluetoothScanning', async function() {
      assertFalse(
          loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'));
      await createPrivacyPageIndex(
          {enableExperimentalWebPlatformFeatures: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
          ['siteSettingsBluetoothScanning'], 'privacy');
    });

    test('RoutingCapturedSurfaceControl', async function() {
      assertFalse(loadTimeData.getBoolean('enableCapturedSurfaceControl'));
      await createPrivacyPageIndex({enableCapturedSurfaceControl: true});
      return testViewsForRoute(
          routes.SITE_SETTINGS_CAPTURED_SURFACE_CONTROL,
          ['siteSettingsCapturedSurfaceControl'], 'privacy');
    });

    test('RoutingFederatedIdentityApi', async function() {
      assertFalse(
          loadTimeData.getBoolean('enableFederatedIdentityApiContentSetting'));
      await createPrivacyPageIndex(
          {enableFederatedIdentityApiContentSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_FEDERATED_IDENTITY_API,
          ['siteSettingsFederatedIdentityApi'], 'privacy');
    });

    test('RoutingFileSystemWriteDetails', async function() {
      assertFalse(loadTimeData.getBoolean('enablePersistentPermissions'));
      await createPrivacyPageIndex({enablePersistentPermissions: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS,
          ['siteSettingsFilesystemWriteDetails'], 'privacy');
    });

    test('RoutingHandTracking', async function() {
      assertFalse(loadTimeData.getBoolean('enableHandTrackingContentSetting'));
      await createPrivacyPageIndex({enableHandTrackingContentSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_HAND_TRACKING, ['siteSettingsHandTracking'],
          'privacy');
    });

    test('RoutingKeyboardLock', async function() {
      assertFalse(loadTimeData.getBoolean('enableKeyboardLockPrompt'));
      await createPrivacyPageIndex({enableKeyboardLockPrompt: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_KEYBOARD_LOCK, ['siteSettingsKeyboardLock'],
          'privacy');
    });

    test('RoutingLocalNetworkAccess', async function() {
      assertFalse(loadTimeData.getBoolean('enableLocalNetworkAccessSetting'));
      await createPrivacyPageIndex({enableLocalNetworkAccessSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_LOCAL_NETWORK_ACCESS,
          ['siteSettingsLocalNetworkAccess'], 'privacy');
    });

    test('RoutingPaymentHandler', async function() {
      assertFalse(
          loadTimeData.getBoolean('enablePaymentHandlerContentSetting'));
      await createPrivacyPageIndex({enablePaymentHandlerContentSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_PAYMENT_HANDLER, ['siteSettingsPaymentHandler'],
          'privacy');
    });

    // <if expr="is_chromeos">
    test('RoutingSmartCardReaders', async function() {
      assertFalse(
          loadTimeData.getBoolean('enableSmartCardReadersContentSetting'));
      await createPrivacyPageIndex(
          {enableSmartCardReadersContentSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_SMART_CARD_READERS,
          ['siteSettingsSmartCardReaders'], 'privacy');
    });
    // </if>

    test('RoutingWebAppInstallation', async function() {
      assertFalse(loadTimeData.getBoolean('enableWebAppInstallation'));
      await createPrivacyPageIndex({enableWebAppInstallation: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_WEB_APP_INSTALLATION,
          ['siteSettingsWebAppInstallation'], 'privacy');
    });

    test('RoutingWebPrinting', async function() {
      assertFalse(loadTimeData.getBoolean('enableWebPrintingContentSetting'));
      await createPrivacyPageIndex({enableWebPrintingContentSetting: true});

      return testViewsForRoute(
          routes.SITE_SETTINGS_WEB_PRINTING, ['siteSettingsWebPrinting'],
          'privacy');
    });
  });
});
