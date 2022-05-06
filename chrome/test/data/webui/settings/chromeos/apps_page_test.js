// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {AndroidAppsBrowserProxyImpl, createBoolPermission, Router, routes, setAppNotificationProviderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

import {TestAndroidAppsBrowserProxy} from './test_android_apps_browser_proxy.js';

/** @type {?OsSettingsAppsPageElement} */
let appsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
let androidAppsBrowserProxy = null;

function getFakePrefs() {
  return {
    arc: {
      enabled: {
        key: 'arc.enabledd',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      }
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 2,
      }
    }
  };
}

function setPrefs(restoreOption) {
  return {
    arc: {
      enabled: {
        key: 'arc.enabledd',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      }
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: restoreOption,
      }
    }
  };
}

class FakeAppNotificationHandler {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private
     *     {?chromeos.settings.appNotification.mojom.
     *      AppNotificationObserverRemote}
     */
    this.appNotificationObserverRemote_;

    /**
     * @private {!Array<!chromeos.settings.appNotification.mojom.App>}
     */
    this.apps_ = [];

    /** @private {boolean} */
    this.isDndEnabled_ = false;

    this.resetForTest();
  }

  resetForTest() {
    if (this.appNotificationObserverRemote_) {
      this.appNotificationObserverRemote_ = null;
    }

    this.resolverMap_.set('addObserver', new PromiseResolver());
    this.resolverMap_.set('getQuietMode', new PromiseResolver());
    this.resolverMap_.set('setQuietMode', new PromiseResolver());
    this.resolverMap_.set('setNotificationPermission', new PromiseResolver());
    this.resolverMap_.set('getApps', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @return
   *      {chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   */
  getObserverRemote() {
    return this.appNotificationObserverRemote_;
  }

  // appNotificationHandler methods

  /**
   * @param {!chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   *      remote
   * @return {!Promise}
   */
  addObserver(remote) {
    return new Promise(resolve => {
      this.appNotificationObserverRemote_ = remote;
      this.methodCalled('addObserver');
      resolve();
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  setQuietMode(enabled) {
    return new Promise(resolve => {
      this.methodCalled('setQuietMode');
      resolve({success: true});
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  getQuietMode() {
    return new Promise(resolve => {
      this.methodCalled('getQuietMode');
      resolve({success: this.isDndEnabled_});
    });
  }

  /**
   * @param {string} id
   * @param {!appManagement.mojom.Permission} permission
   */
  setNotificationPermission(id, permission) {
    return new Promise(resolve => {
      this.methodCalled('setNotificationPermission');
      resolve({success: true});
    });
  }

  /**
   * @return {!Promise<!Array<!chromeos.settings.appNotification.mojom.App>>}
   */
  getApps() {
    return new Promise(resolve => {
      this.methodCalled('getApps');
      resolve({apps: this.apps_});
    });
  }
}

suite('AppsPageTests', function() {
  /**
   * @type {
   *    ?chromeos.settings.appNotification.mojom.AppNotificationHandlerRemote
   *  }
   */
  let mojoApi_;

  /**
   * @param {string} id
   * @param {string} title
   * @param {!appManagement.mojom.Permission} permission
   * @param {?chromeos.settings.appNotification.mojom.Readiness} readiness
   * @return {!chromeos.settings.appNotification.mojom.App}
   */
  function createApp(
      id, title, permission,
      readiness = chromeos.settings.appNotification.mojom.Readiness.kReady) {
    return {
      id: id,
      title: title,
      notificationPermission: permission,
      readiness: readiness
    };
  }

  /**
   * @return {!Promise}
   */
  function initializeObserver() {
    return mojoApi_.whenCalled('addObserver');
  }

  /** @param {!Array<!chromeos.settings.appNotification.mojom.App>} */
  function simulateNotificationAppChanged(app) {
    mojoApi_.getObserverRemote().onNotificationAppChanged(app);
  }

  setup(async () => {
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstance(androidAppsBrowserProxy);
    PolymerTest.clearBody();
    mojoApi_ = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi_);
    appsPage = document.createElement('os-settings-apps-page');

    document.body.appendChild(appsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    appsPage.remove();
    appsPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  suite('Page Combinations', function() {
    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = getFakePrefs();
    });

    const AndroidAppsShown = () => !!appsPage.$$('#android-apps');
    const AppManagementShown = () => !!appsPage.$$('#appManagement');
    const RestoreAppsOnStartupShown = () => !!appsPage.$$('#onStartupDropdown');

    test('Only App Management Shown', function() {
      appsPage.showAndroidApps = false;
      appsPage.showStartup = false;
      flush();

      assertTrue(AppManagementShown());
      assertFalse(AndroidAppsShown());
      assertFalse(RestoreAppsOnStartupShown());
    });

    test('Android Apps and App Management Shown', function() {
      appsPage.showAndroidApps = true;
      appsPage.showStartup = false;
      flush();

      assertTrue(AppManagementShown());
      assertTrue(AndroidAppsShown());
      assertFalse(RestoreAppsOnStartupShown());
    });

    test('Android Apps, On Startup and App Management Shown', function() {
      appsPage.showAndroidApps = true;
      appsPage.showStartup = true;
      flush();

      assertTrue(AppManagementShown());
      assertTrue(AndroidAppsShown());
      assertTrue(RestoreAppsOnStartupShown());
      assertEquals(3, appsPage.onStartupOptions_.length);
    });

    test('App notification row', async () => {
      appsPage.showAndroidApps = true;
      appsPage.showStartup = true;
      flush();

      const rowLink = appsPage.$$('#appNotifications');
      assertTrue(!!rowLink);
      // Test default is to have 0 apps.
      assertEquals('0 apps', rowLink.subLabel);

      const permission1 = createBoolPermission(
          /**id=*/ 1,
          /**value=*/ false, /**is_managed=*/ false);
      const permission2 = createBoolPermission(
          /**id=*/ 2,
          /**value=*/ true, /**is_managed=*/ false);
      const app1 = createApp('1', 'App1', permission1);
      const app2 = createApp('2', 'App2', permission2);

      simulateNotificationAppChanged(app1);
      simulateNotificationAppChanged(app2);
      await flushTasks();

      assertEquals('2 apps', rowLink.subLabel);

      // Simulate an uninstalled app.
      const app3 = createApp(
          '2', 'App2', permission2,
          chromeos.settings.appNotification.mojom.Readiness.kUninstalledByUser);
      simulateNotificationAppChanged(app3);
      await flushTasks();
      assertEquals('1 apps', rowLink.subLabel);
    });
  });

  suite('Main Page', function() {
    setup(function() {
      appsPage.showAndroidApps = true;
      appsPage.havePlayStoreApp = true;
      appsPage.showStartup = true;
      appsPage.prefs = getFakePrefs();
      appsPage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: false,
      };
      flush();
    });

    test('Clicking enable button enables ARC', function() {
      const button = appsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!appsPage.$$('.subpage-arrow'));

      button.click();
      flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      appsPage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(!!appsPage.$$('.subpage-arrow'));
    });

    test('On startup dropdown menu', async () => {
      appsPage.prefs = setPrefs(1);
      flush();
      assertEquals(1, appsPage.$$('#onStartupDropdown').pref.value);

      appsPage.prefs = setPrefs(2);
      flush();
      assertEquals(2, appsPage.$$('#onStartupDropdown').pref.value);

      appsPage.prefs = setPrefs(3);
      flush();
      assertEquals(3, appsPage.$$('#onStartupDropdown').pref.value);
    });

    test('Deep link to On startup dropdown menu', async () => {
      flush();

      const params = new URLSearchParams();
      params.append('settingId', '703');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement = appsPage.$$('#onStartupDropdown')
                                  .shadowRoot.querySelector('#dropdownMenu');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'On startup dropdown menu should be focused for settingId=703.');
    });

    test('Deep link to manage android prefs', async () => {
      appsPage.havePlayStoreApp = false;
      flush();

      const params = new URLSearchParams();
      params.append('settingId', '700');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement =
          appsPage.$$('#manageApps').shadowRoot.querySelector('cr-icon-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to turn on Play Store', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '702');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement = appsPage.$$('#enable');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Turn on play store button should be focused for settingId=702.');
    });

    // TODO(crbug.com/1006662): Test that setting playStoreEnabled to false
    // navigates back to the main apps section.
  });

  suite('Android apps subpage', function() {
    let subpage = null;

    setup(function() {
      androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
      AndroidAppsBrowserProxyImpl.setInstance(androidAppsBrowserProxy);
      PolymerTest.clearBody();
      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);
      testing.Test.disableAnimationsAndTransitions();

      // Because we can't simulate the loadTimeData value androidAppsVisible,
      // this route doesn't exist for tests. Add it in for testing.
      if (!routes.ANDROID_APPS_DETAILS) {
        routes.ANDROID_APPS_DETAILS = routes.APPS.createChild(
            '/' + chromeos.settings.mojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH);
      }

      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
    });

    teardown(function() {
      subpage.remove();
      subpage = null;
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#remove'));
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsUpdate', function() {
      assertTrue(!subpage.$$('#manageApps'));
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      assertTrue(!!subpage.$$('#manageApps'));

      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      const button = subpage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      flush();
      return promise;
    });

    test('Disable', function() {
      const dialog = subpage.$$('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      const remove = subpage.$$('#remove');
      assertTrue(!!remove);

      subpage.onRemoveTap_();
      flush();
      assertTrue(dialog.open);
      dialog.close();
    });

    test('ARC enabled by policy', function() {
      subpage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED
          }
        }
      };
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();

      assertFalse(!!subpage.$$('#remove'));
      assertTrue(!!subpage.$$('#manageApps'));
    });

    test('Can open app settings without Play Store', function() {
      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const button = subpage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      flush();
      return promise;
    });

    test('Deep link to manage android prefs - subpage', async () => {
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const params = new URLSearchParams();
      params.append('settingId', '700');
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement =
          subpage.$$('#manageApps').shadowRoot.querySelector('cr-icon-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to remove play store', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '701');
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement = subpage.$$('#remove cr-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Remove play store button should be focused for settingId=701.');
    });

    test('ManageUsbDevice', function() {
      // ARCVM is not enabled
      subpage.showArcvmManageUsb = false;
      flush();
      assertFalse(!!subpage.$$('#manageArcvmShareUsbDevices'));

      // ARCMV is enabled
      subpage.showArcvmManageUsb = true;
      flush();
      assertTrue(!!subpage.$$('#manageArcvmShareUsbDevices'));
    });
  });
});
