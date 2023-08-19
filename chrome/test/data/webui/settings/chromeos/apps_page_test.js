// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AndroidAppsBrowserProxyImpl, appNotificationHandlerMojom, createRouterForTesting, Router, routes, routesMojom, setAppNotificationProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestAndroidAppsBrowserProxy} from './test_android_apps_browser_proxy.js';

const {App, AppNotificationsObserverRemote, Readiness} =
    appNotificationHandlerMojom;

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
      },
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 2,
      },
    },
  };
}

function setPrefs(restoreOption) {
  return {
    arc: {
      enabled: {
        key: 'arc.enabledd',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: restoreOption,
      },
    },
  };
}

/**
 * @param {string} id
 * @param {string} title
 * @param {!Permission} permission
 * @param {?Readiness} readiness
 * @return {!App}
 */
function createApp(id, title, permission, readiness = Readiness.kReady) {
  return {
    id: id,
    title: title,
    notificationPermission: permission,
    readiness: readiness,
  };
}

class FakeAppNotificationHandler {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private {?AppNotificationsObserverRemote}
     */
    this.appNotificationsObserverRemote_;

    /**
     * @private {!Array<!App>}
     */
    this.apps_ = [];

    /** @private {boolean} */
    this.isDndEnabled_ = false;

    this.resetForTest();
  }

  resetForTest() {
    if (this.appNotificationsObserverRemote_) {
      this.appNotificationsObserverRemote_ = null;
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
   * @return {AppNotificationsObserverRemote}
   */
  getObserverRemote() {
    return this.appNotificationsObserverRemote_;
  }

  // appNotificationHandler methods

  /**
   * @param {!AppNotificationsObserverRemote} remote
   * @return {!Promise}
   */
  addObserver(remote) {
    return new Promise(resolve => {
      this.appNotificationsObserverRemote_ = remote;
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
   * @param {!Permission} permission
   */
  setNotificationPermission(id, permission) {
    return new Promise(resolve => {
      this.methodCalled('setNotificationPermission');
      resolve({success: true});
    });
  }

  /**
   * @return {!Promise<!Array<!App>>}
   */
  getApps() {
    return new Promise(resolve => {
      this.methodCalled('getApps');
      resolve({apps: this.apps_});
    });
  }
}

suite('<os-apps-page> available settings rows', () => {
  /** @type {OsSettingsAppsPageElement} */
  let appsPage;

  function initPage() {
    loadTimeData.overrideValues({isPlayStoreAvailable: true});
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    document.body.appendChild(appsPage);
    flush();
  }

  setup(async () => {
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
    PolymerTest.clearBody();
  });

  teardown(() => {
    appsPage.remove();
    appsPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  const queryAndroidAppsRow = () =>
      appsPage.shadowRoot.querySelector('#androidApps');
  const queryAppManagementRow = () =>
      appsPage.shadowRoot.querySelector('#appManagementRow');
  const queryAppsOnStartupRow = () =>
      appsPage.shadowRoot.querySelector('#onStartupDropdown');

  test('Only App Management is shown', () => {
    loadTimeData.overrideValues({
      showStartup: false,
      androidAppsVisible: false,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertEquals(null, queryAndroidAppsRow());
    assertEquals(null, queryAppsOnStartupRow());
  });

  test('Android Apps and App Management are shown', () => {
    loadTimeData.overrideValues({
      showStartup: false,
      androidAppsVisible: true,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertTrue(!!queryAndroidAppsRow());
    assertEquals(null, queryAppsOnStartupRow());
  });

  test('Android Apps, On Startup, and App Management are shown', () => {
    loadTimeData.overrideValues({
      showStartup: true,
      androidAppsVisible: true,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertTrue(!!queryAndroidAppsRow());
    assertTrue(!!queryAppsOnStartupRow());
    assertEquals(3, appsPage.onStartupOptions_.length);
  });
});

suite('<os-apps-page> Subpage trigger focusing', () => {
  /** @type {OsSettingsAppsPageElement} */
  let appsPage;

  function initPage() {
    Router.getInstance().navigateTo(routes.APPS);
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    appsPage.androidAppsInfo = {
      playStoreEnabled: true,
    };
    document.body.appendChild(appsPage);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({
      androidAppsVisible: true,
      showOsSettingsAppNotificationsRow: true,
    });

    // Reinitialize Router and routes based on load time data
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
    PolymerTest.clearBody();
  });

  teardown(() => {
    appsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  [{
    triggerSelector: '#appManagementRow',
    routeName: 'APP_MANAGEMENT',
  },
   {
     triggerSelector: '#appNotificationsRow',
     routeName: 'APP_NOTIFICATIONS',
   },
   {
     triggerSelector: '#androidApps .subpage-arrow',
     routeName: 'ANDROID_APPS_DETAILS',
   },
  ].forEach(({triggerSelector, routeName}) => {
    test(
        `${routeName} subpage trigger is focused when returning from subpage`,
        async () => {
          initPage();

          const subpageTrigger =
              appsPage.shadowRoot.querySelector(triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to Detailed build info subpage
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(appsPage);

          assertEquals(
              subpageTrigger, appsPage.shadowRoot.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});

suite('AppsPageTests', function() {
  /**
   * @type {
   *    ?AppNotificationHandlerRemote
   *  }
   */
  let mojoApi_;

  /**
   * @return {!Promise}
   */
  function initializeObserver() {
    return mojoApi_.whenCalled('addObserver');
  }

  /** @param {!Array<!App>} */
  function simulateNotificationAppChanged(app) {
    mojoApi_.getObserverRemote().onNotificationAppChanged(app);
  }

  setup(async () => {
    loadTimeData.overrideValues({
      showOsSettingsAppNotificationsRow: true,
      isPlayStoreAvailable: true,
      androidAppsVisible: true,
    });
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
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

  suite('Main Page', function() {
    setup(function() {
      appsPage.prefs = getFakePrefs();
      appsPage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: false,
      };
      flush();
    });

    test('App notification row', async () => {
      const rowLink = appsPage.shadowRoot.querySelector('#appNotificationsRow');
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
      const app3 =
          createApp('2', 'App2', permission2, Readiness.kUninstalledByUser);
      simulateNotificationAppChanged(app3);
      await flushTasks();
      assertEquals('1 apps', rowLink.subLabel);
    });

    test('Clicking enable button enables ARC', function() {
      const button = appsPage.shadowRoot.querySelector('#enable');
      assertTrue(!!button);
      assertFalse(!!appsPage.shadowRoot.querySelector('.subpage-arrow'));

      button.click();
      flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      appsPage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(!!appsPage.shadowRoot.querySelector('.subpage-arrow'));
    });

    test('On startup dropdown menu', async () => {
      appsPage.prefs = setPrefs(1);
      flush();
      assertEquals(
          1,
          appsPage.shadowRoot.querySelector('#onStartupDropdown').pref.value);

      appsPage.prefs = setPrefs(2);
      flush();
      assertEquals(
          2,
          appsPage.shadowRoot.querySelector('#onStartupDropdown').pref.value);

      appsPage.prefs = setPrefs(3);
      flush();
      assertEquals(
          3,
          appsPage.shadowRoot.querySelector('#onStartupDropdown').pref.value);
    });

    test('Deep link to On startup dropdown menu', async () => {
      flush();

      const params = new URLSearchParams();
      params.append('settingId', '703');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement =
          appsPage.shadowRoot.querySelector('#onStartupDropdown')
              .shadowRoot.querySelector('#dropdownMenu');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'On startup dropdown menu should be focused for settingId=703.');
    });

    test('Deep link to manage android prefs', async () => {
      // Simulate showing manage apps link
      appsPage.set('isPlayStoreAvailable_', false);
      flush();

      const params = new URLSearchParams();
      params.append('settingId', '700');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement = appsPage.shadowRoot.querySelector('#manageApps')
                                  .shadowRoot.querySelector('cr-icon-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to turn on Play Store', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '702');
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement = appsPage.shadowRoot.querySelector('#enable');
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
      AndroidAppsBrowserProxyImpl.setInstanceForTesting(
          androidAppsBrowserProxy);
      PolymerTest.clearBody();
      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);
      testing.Test.disableAnimationsAndTransitions();

      // Because we can't simulate the loadTimeData value androidAppsVisible,
      // this route doesn't exist for tests. Add it in for testing.
      if (!routes.ANDROID_APPS_DETAILS) {
        routes.ANDROID_APPS_DETAILS = routes.APPS.createChild(
            '/' + routesMojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH);
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
      assertTrue(!!subpage.shadowRoot.querySelector('#remove'));
      assertTrue(!subpage.shadowRoot.querySelector('#manageApps'));
    });

    test('ManageAppsUpdate', function() {
      assertTrue(!subpage.shadowRoot.querySelector('#manageApps'));
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      assertTrue(!!subpage.shadowRoot.querySelector('#manageApps'));

      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(!subpage.shadowRoot.querySelector('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      const button = subpage.shadowRoot.querySelector('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      flush();
      return promise;
    });

    test('Disable', function() {
      const dialog = subpage.shadowRoot.querySelector('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      const remove = subpage.shadowRoot.querySelector('#remove');
      assertTrue(!!remove);

      const button = remove.querySelector('cr-button');
      assertTrue(!!button);
      button.click();
      flush();

      assertTrue(dialog.open);
      dialog.close();
    });

    test('ARC enabled by policy', function() {
      subpage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          },
        },
      };
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();

      assertFalse(!!subpage.shadowRoot.querySelector('#remove'));
      assertTrue(!!subpage.shadowRoot.querySelector('#manageApps'));
    });

    test('Can open app settings without Play Store', function() {
      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const button = subpage.shadowRoot.querySelector('#manageApps');
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

      const deepLinkElement = subpage.shadowRoot.querySelector('#manageApps')
                                  .shadowRoot.querySelector('cr-icon-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to remove play store', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '701');
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement =
          subpage.shadowRoot.querySelector('#remove cr-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Remove play store button should be focused for settingId=701.');
    });

    test('ManageUsbDevice', function() {
      // ARCVM is not enabled
      subpage.isArcVmManageUsbAvailable = false;
      flush();
      assertFalse(
          !!subpage.shadowRoot.querySelector('#manageArcvmShareUsbDevices'));

      // ARCMV is enabled
      subpage.isArcVmManageUsbAvailable = true;
      flush();
      assertTrue(
          !!subpage.shadowRoot.querySelector('#manageArcvmShareUsbDevices'));
    });
  });
});
