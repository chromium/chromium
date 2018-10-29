// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

const setCrostiniPrefs = function(enabled, opt_sharedPaths) {
  crostiniPage.prefs = {
    crostini: {
      enabled: {value: enabled},
      shared_paths: {value: opt_sharedPaths || []}
    }
  };
  crostiniBrowserProxy.enabled = enabled;
  crostiniBrowserProxy.sharedPaths = opt_sharedPaths || [];
  Polymer.dom.flush();
};

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      crostiniPage.async(resolve);
    });
  }

  suite('Main Page', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));

      button.click();
      Polymer.dom.flush();
      setCrostiniPrefs(crostiniBrowserProxy.enabled);
      assertTrue(crostiniPage.prefs.crostini.enabled.value);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });
  });

  suite('SubPageDetails', function() {
    let subpage;

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
      setCrostiniPrefs(true);
      settings.navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-subpage');
        assertTrue(!!subpage);
      });
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths'));
      assertTrue(!!subpage.$$('#remove'));
    });

    test('SharedPaths', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths .subpage-arrow'));
      subpage.$$('#crostini-shared-paths .subpage-arrow').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-shared-paths');
        assertTrue(!!subpage);
      });
    });


    test('Remove', function() {
      assertTrue(!!subpage.$$('#remove .subpage-arrow'));
      subpage.$$('#remove .subpage-arrow').click();
      setCrostiniPrefs(crostiniBrowserProxy.enabled);
      assertFalse(crostiniPage.prefs.crostini.enabled.value);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });
    });

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.CROSTINI_DETAILS);
      setCrostiniPrefs(false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
      });
    });
  });

  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(function() {
      setCrostiniPrefs(true, crostiniBrowserProxy.sharedPaths);
      return flushAsync().then(() => {
        settings.navigateTo(settings.routes.CROSTINI_SHARED_PATHS);
        return flushAsync().then(() => {
          subpage = crostiniPage.$$('settings-crostini-shared-paths');
          assertTrue(!!subpage);
        });
      });
    });

    test('Sanity', function() {
      assertEquals(
          2,
          Polymer.dom(subpage.root).querySelectorAll('.settings-box').length);
    });

    test('Remove', function() {
      assertTrue(!!subpage.$$('.settings-box button'));
      subpage.$$('.settings-box button').click();
      assertEquals(1, crostiniBrowserProxy.sharedPaths.length);
      setCrostiniPrefs(true, crostiniBrowserProxy.sharedPaths);
      return flushAsync().then(() => {
        Polymer.dom.flush();
        assertEquals(
            1,
            Polymer.dom(subpage.root).querySelectorAll('.settings-box').length);
      });
    });
  });
});
