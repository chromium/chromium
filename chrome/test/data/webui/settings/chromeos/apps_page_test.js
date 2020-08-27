// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?OsSettingsAppsPageElement} */
let appsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
let androidAppsBrowserProxy = null;

suite('AppsPageTests', function() {
  setup(function() {
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    settings.AndroidAppsBrowserProxyImpl.instance_ = androidAppsBrowserProxy;
    PolymerTest.clearBody();
    appsPage = document.createElement('os-settings-apps-page');
    document.body.appendChild(appsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    appsPage.remove();
    appsPage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('Page Combinations', function() {
    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {arc: {enabled: {value: false}}};
    });

    const AndroidAppsShown = () => !!appsPage.$$('#android-apps');
    const AppManagementShown = () => !!appsPage.$$('#appManagement');

    test('Only App Management Shown', function() {
      appsPage.showAndroidApps = false;
      Polymer.dom.flush();

      assertTrue(AppManagementShown());
      assertFalse(AndroidAppsShown());
    });

    test('Android Apps and App Management Shown', function() {
      appsPage.showAndroidApps = true;
      Polymer.dom.flush();

      assertTrue(AppManagementShown());
      assertTrue(AndroidAppsShown());
    });
  });

  suite('Main Page', function() {
    setup(function() {
      appsPage.showAndroidApps = true;
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {arc: {enabled: {value: false}}};
      appsPage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: false,
      };
      Polymer.dom.flush();
    });

    test('Clicking enable button enables ARC', function() {
      const button = appsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!appsPage.$$('.subpage-arrow'));

      button.click();
      Polymer.dom.flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      appsPage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      Polymer.dom.flush();
      assertTrue(!!appsPage.$$('.subpage-arrow'));
    });

    test('Deep link to manage android prefs', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      appsPage.havePlayStoreApp = false;
      Polymer.dom.flush();

      const params = new URLSearchParams;
      params.append('settingId', '700');
      settings.Router.getInstance().navigateTo(settings.routes.APPS, params);

      const deepLinkElement = appsPage.$$('#manageApps').$$('cr-icon-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to turn on Play Store', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      const params = new URLSearchParams;
      params.append('settingId', '702');
      settings.Router.getInstance().navigateTo(settings.routes.APPS, params);

      const deepLinkElement = appsPage.$$('#enable');
      await test_util.waitAfterNextRender(deepLinkElement);
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
      settings.AndroidAppsBrowserProxyImpl.instance_ = androidAppsBrowserProxy;
      PolymerTest.clearBody();
      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);
      testing.Test.disableAnimationsAndTransitions();

      // Because we can't simulate the loadTimeData value androidAppsVisible,
      // this route doesn't exist for tests. Add it in for testing.
      if (!settings.routes.ANDROID_APPS_DETAILS) {
        settings.routes.ANDROID_APPS_DETAILS = settings.routes.APPS.createChild(
            '/' + chromeos.settings.mojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH);
      }

      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      Polymer.dom.flush();
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
      Polymer.dom.flush();
      assertTrue(!!subpage.$$('#manageApps'));

      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      Polymer.dom.flush();
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      Polymer.dom.flush();
      const button = subpage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      Polymer.dom.flush();
      return promise;
    });

    test('Disable', function() {
      const dialog = subpage.$$('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      const remove = subpage.$$('#remove');
      assertTrue(!!remove);

      subpage.onRemoveTap_();
      Polymer.dom.flush();
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
      Polymer.dom.flush();

      assertFalse(!!subpage.$$('#remove'));
      assertTrue(!!subpage.$$('#manageApps'));
    });

    test('Can open app settings without Play Store', function() {
      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      Polymer.dom.flush();

      const button = subpage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      Polymer.dom.flush();
      return promise;
    });

    test('Deep link to manage android prefs - subpage', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      Polymer.dom.flush();

      const params = new URLSearchParams;
      params.append('settingId', '700');
      settings.Router.getInstance().navigateTo(
          settings.routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement = subpage.$$('#manageApps').$$('cr-icon-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Manage android prefs button should be focused for settingId=700.');
    });

    test('Deep link to remove play store', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      const params = new URLSearchParams;
      params.append('settingId', '701');
      settings.Router.getInstance().navigateTo(
          settings.routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement = subpage.$$('#remove cr-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Remove play store button should be focused for settingId=701.');
    });
  });
});
