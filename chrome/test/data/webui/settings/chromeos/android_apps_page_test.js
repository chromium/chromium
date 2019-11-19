// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Changes to this file should be reflected in android_apps_page_test.js

// TODO(crbug.com/1006152): Delete this file once the split-settings flag is
// removed.

/** @type {?SettingsAndroidAppsPageElement} */
let androidAppsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
let androidAppsBrowserProxy = null;

const setAndroidAppsState = function(playStoreEnabled, settingsAppAvailable) {
  const appsInfo = {
    playStoreEnabled: playStoreEnabled,
    settingsAppAvailable: settingsAppAvailable,
  };
  androidAppsPage.androidAppsInfo = appsInfo;
  Polymer.dom.flush();
};

suite('AndroidAppsPageTests', function() {
  setup(function() {
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    settings.AndroidAppsBrowserProxyImpl.instance_ = androidAppsBrowserProxy;
    PolymerTest.clearBody();
    androidAppsPage = document.createElement('settings-android-apps-page');
    document.body.appendChild(androidAppsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    androidAppsPage.remove();
  });

  suite('Main Page', function() {
    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {arc: {enabled: {value: false}}};
      setAndroidAppsState(false, false);
    });

    test('Enable', function() {
      const button = androidAppsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!androidAppsPage.$$('.subpage-arrow'));

      button.click();
      Polymer.dom.flush();
      assertTrue(androidAppsPage.prefs.arc.enabled.value);

      setAndroidAppsState(true, false);
      assertTrue(!!androidAppsPage.$$('.subpage-arrow'));
    });
  });

  // TODO(crbug.com/1006662): Fix test suite.
  suite('SubPage', function() {
    let subpage;

    function flushAsync() {
      Polymer.dom.flush();
      return new Promise(resolve => {
        androidAppsPage.async(resolve);
      });
    }

    /**
     * Returns a new promise that resolves after a window 'popstate' event.
     * @return {!Promise}
     */
    function whenPopState() {
      return new Promise(function(resolve) {
        window.addEventListener('popstate', function callback() {
          window.removeEventListener('popstate', callback);
          resolve();
        });
      });
    }

    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, false);
      settings.navigateTo(settings.routes.ANDROID_APPS);
      androidAppsPage.$$('#android-apps').click();
      return flushAsync().then(() => {
        subpage = androidAppsPage.$$('settings-android-apps-subpage');
        assertTrue(!!subpage);
      });
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#remove'));
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsUpdate', function() {
      assertTrue(!subpage.$$('#manageApps'));
      setAndroidAppsState(true, true);
      assertTrue(!!subpage.$$('#manageApps'));
      setAndroidAppsState(true, false);
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      setAndroidAppsState(true, true);
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

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.ANDROID_APPS_DETAILS);
      setAndroidAppsState(false, false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.ANDROID_APPS);
      });
    });
  });

  // TODO(crbug.com/1006662): Fix test suite.
  suite('Enforced', function() {
    let subpage;

    setup(function() {
      androidAppsPage.havePlayStoreApp = true;
      androidAppsPage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED
          }
        }
      };
      setAndroidAppsState(true, true);
      androidAppsPage.$$('#android-apps').click();
      Polymer.dom.flush();
      subpage = androidAppsPage.$$('settings-android-apps-subpage');
      assertTrue(!!subpage);
    });

    test('Sanity', function() {
      Polymer.dom.flush();
      assertFalse(!!subpage.$$('#remove'));
      assertTrue(!!subpage.$$('#manageApps'));
    });
  });

  suite('NoPlayStore', function() {
    setup(function() {
      androidAppsPage.havePlayStoreApp = false;
      androidAppsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, true);
    });

    test('Sanity', function() {
      assertTrue(!!androidAppsPage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      const button = androidAppsPage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      Polymer.dom.flush();
      return promise;
    });
  });
});
