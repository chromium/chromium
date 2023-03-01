// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appNotificationHandlerMojom, setAppNotificationProviderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {createBoolPermission, getBoolPermissionValue, isBoolValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const {App, AppNotificationsObserverRemote, Readiness} =
    appNotificationHandlerMojom;

class FakeAppNotificationHandler {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private {?AppNotificationsObserverRemote}
     */
    this.appNotificationsObserverRemote_;

    this.isQuietModeEnabled_ = false;

    this.lastUpdatedAppId_ = -1;

    this.lastUpdatedAppPermission_ = {};

    /**
     * @private {!Array<!App>}
     */
    this.apps_ = [];

    this.resetForTest();
  }

  resetForTest() {
    if (this.appNotificationsObserverRemote_) {
      this.appNotificationsObserverRemote_ = null;
    }

    this.apps_ = [];
    this.isQuietModeEnabled_ = false;
    this.lastUpdatedAppId_ = -1;
    this.lastUpdatedAppPermission_ = {};

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

  /**
   * @return {boolean} quietModeState
   */
  getCurrentQuietModeState() {
    return this.isQuietModeEnabled_;
  }

  /**
   * @return {number}
   */
  getLastUpdatedAppId() {
    return this.lastUpdatedAppId_;
  }

  /**
   * @return {!Permission}
   */
  getLastUpdatedPermission() {
    return this.lastUpdatedAppPermission_;
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
  getQuietMode() {
    return new Promise(resolve => {
      this.methodCalled('getQuietMode');
      resolve({success: this.isQuietModeEnabled_});
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  setQuietMode(enabled) {
    this.isQuietModeEnabled_ = enabled;
    return new Promise(resolve => {
      this.methodCalled('setQuietMode');
      resolve({success: true});
    });
  }

  /**
   * @param {string} id
   * @param {!Permission} permission
   */
  setNotificationPermission(id, permission) {
    return new Promise(resolve => {
      this.lastUpdatedAppId_ = id;
      this.lastUpdatedAppPermission_ = permission;
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

suite('AppNotificationsSubpageTests', function() {
  /** @type {AppNotificationsSubpage} */
  let page;

  /**
   * @type {
   *    ?AppNotificationHandlerRemote
   *  }
   */
  let mojoApi_;

  let setQuietModeCounter = 0;

  function createPage() {
    page = document.createElement('settings-app-notifications-subpage');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
  }

  suiteSetup(() => {
    mojoApi_ = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi_);
  });

  setup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page.remove();
    page = null;
  });

  /**
   * @return {!Promise}
   */
  function initializeObserver() {
    return mojoApi_.whenCalled('addObserver');
  }

  function simulateQuickSettings(enable) {
    mojoApi_.getObserverRemote().onQuietModeChanged(enable);
  }

  /** @param {!App} */
  function simulateNotificationAppChanged(app) {
    mojoApi_.getObserverRemote().onNotificationAppChanged(app);
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

  test('loadAppListAndClickToggle', async () => {
    createPage();
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('1', 'App1', permission1);
    const app2 = createApp('2', 'App2', permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    // Expect 2 apps to be loaded.
    const appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(2, appRowList.length);

    const appRow1 = appRowList[0];
    assertFalse(appRow1.$.appToggle.checked);
    assertFalse(appRow1.$.appToggle.disabled);
    assertEquals('App1', appRow1.$.appTitle.textContent.trim());

    const appRow2 = appRowList[1];
    assertTrue(appRow2.$.appToggle.checked);
    assertFalse(appRow2.$.appToggle.disabled);
    assertEquals('App2', appRow2.$.appTitle.textContent.trim());

    // Click on the first app's toggle.
    appRow1.$.appToggle.click();

    await mojoApi_.whenCalled('setNotificationPermission');

    // Verify that the sent message matches the app it was clicked from.
    assertEquals('1', mojoApi_.getLastUpdatedAppId());
    const lastUpdatedPermission = mojoApi_.getLastUpdatedPermission();
    assertEquals(1, lastUpdatedPermission.permissionType);
    assertTrue(isBoolValue(lastUpdatedPermission.value));
    assertEquals(false, lastUpdatedPermission.isManaged);
    assertTrue(getBoolPermissionValue(lastUpdatedPermission.value));
  });

  test('RemovedApp', async () => {
    createPage();
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('1', 'App1', permission1);
    const app2 = createApp('2', 'App2', permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    // Expect 2 apps to be loaded.
    let appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(2, appRowList.length);

    const app3 =
        createApp('1', 'App1', permission1, Readiness.kUninstalledByUser);
    simulateNotificationAppChanged(app3);

    await flushTasks();
    // Expect only 1 app to be shown now.
    appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(1, appRowList.length);

    const appRow1 = appRowList[0];
    assertEquals('App2', appRow1.$.appTitle.textContent.trim());
  });

  test('Each app-notification-row displays correctly', async () => {
    createPage();
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ true);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('file-id', appTitle1, permission1);
    const app2 = createApp('chrome-id', appTitle2, permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    const chromeRow = page.$.appNotificationsList.children[0];
    const filesRow = page.$.appNotificationsList.children[1];

    assertTrue(!!page);
    flush();

    // Apps should be listed in alphabetical order. |appTitle1| should come
    // before |appTitle2|, so a 1 should be returned by localCompare.
    const expected = 1;
    const actual = appTitle1.localeCompare(appTitle2);
    assertEquals(expected, actual);

    assertEquals(appTitle2, chromeRow.$.appTitle.textContent.trim());
    assertFalse(chromeRow.$.appToggle.disabled);
    assertFalse(!!chromeRow.shadowRoot.querySelector('cr-policy-indicator'));

    assertEquals(appTitle1, filesRow.$.appTitle.textContent.trim());
    assertTrue(filesRow.$.appToggle.disabled);
    assertTrue(!!filesRow.shadowRoot.querySelector('cr-policy-indicator'));
  });

  test('toggleDoNotDisturb', function() {
    createPage();
    const dndToggle = page.$.doNotDisturbToggle;
    assertTrue(!!dndToggle);

    flush();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());

    dndToggle.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi_.getCurrentQuietModeState());

    // Click again will change the value.
    dndToggle.click();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());
  });

  test('SetQuietMode being called correctly', function() {
    createPage();
    const dndToggle = page.$.doNotDisturbToggle;
    // Click the toggle button a certain count.
    const testCount = 3;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that every toggle click makes a call to setQuietMode and is
    // counted accordingly.
    for (let i = 0; i < testCount; i++) {
      return initializeObserver()
          .then(() => {
            flush();
            dndToggle.click();
            return mojoApi_.whenCalled('setQuietMode').then(() => {
              setQuietModeCounter++;
            });
          })
          .then(() => {
            flush();
            assertEquals(i + 1, setQuietModeCounter);
          });
    }
  });

  test('changing toggle with OnQuietModeChanged', function() {
    createPage();
    const dndToggle = page.$.doNotDisturbToggle;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that calling onQuietModeChanged sets toggle value.
    // This is equivalent to changing the DoNotDisturb setting in quick
    // settings.
    return initializeObserver()
        .then(() => {
          flush();
          simulateQuickSettings(/** enable */ true);
          return flushTasks();
        })
        .then(() => {
          assertTrue(dndToggle.checked);
        });
  });

  suite('App badging', () => {
    function makeFakePrefs(appBadgingEnabled = false) {
      return {
        ash: {
          app_notification_badging_enabled: {
            key: 'ash.app_notification_badging_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: appBadgingEnabled,
          },
        },
      };
    }

    setup(() => {
      loadTimeData.overrideValues({showOsSettingsAppBadgingToggle: true});
    });

    test('App badging toggle is not visible if feature is disabled', () => {
      loadTimeData.overrideValues({showOsSettingsAppBadgingToggle: false});
      createPage();

      const appBadgingToggle =
          page.shadowRoot.querySelector('#appBadgingToggleButton');
      assertEquals(null, appBadgingToggle);
    });

    test('App badging toggle is visible if the feature is enabled', () => {
      createPage();
      const appBadgingToggle =
          page.shadowRoot.querySelector('#appBadgingToggleButton');
      assertTrue(!!appBadgingToggle);
    });

    test('Clicking the app badging button toggles the pref value', () => {
      createPage();
      page.prefs = makeFakePrefs(true);

      const appBadgingToggle =
          page.shadowRoot.querySelector('#appBadgingToggleButton');
      assertTrue(appBadgingToggle.checked);
      assertTrue(page.prefs.ash.app_notification_badging_enabled.value);

      appBadgingToggle.click();
      assertFalse(appBadgingToggle.checked);
      assertFalse(page.prefs.ash.app_notification_badging_enabled.value);

      appBadgingToggle.click();
      assertTrue(appBadgingToggle.checked);
      assertTrue(page.prefs.ash.app_notification_badging_enabled.value);
    });
  });
});
