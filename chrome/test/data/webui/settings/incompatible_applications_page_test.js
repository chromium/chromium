// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.IncompatibleApplicationsBrowserProxy} */
class TestIncompatibleApplicationsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestIncompatibleApplicationsList',
      'startApplicationUninstallation',
      'openURL',
      'getSubtitlePluralString',
      'getSubtitleNoAdminRightsPluralString',
      'getListTitlePluralString',
    ]);

    /** @private {!Array<!settings.IncompatibleApplication>} */
    this.incompatibleApplications_ = [];
  }

  /** @override */
  requestIncompatibleApplicationsList() {
    this.methodCalled('requestIncompatibleApplicationsList');
    return Promise.resolve(this.incompatibleApplications_);
  }

  /** @override */
  startApplicationUninstallation(applicationName) {
    this.methodCalled('startApplicationUninstallation', applicationName);
  }

  /** @override */
  openURL(url) {
    this.methodCalled('openURL', url);
  }

  /** @override */
  getSubtitlePluralString(numApplications) {
    this.methodCalled('getSubtitlePluralString');
    return Promise.resolve('');
  }

  /** @override */
  getSubtitleNoAdminRightsPluralString(numApplications) {
    this.methodCalled('getSubtitleNoAdminRightsPluralString');
    return Promise.resolve('');
  }

  /** @override */
  getListTitlePluralString(numApplications) {
    this.methodCalled('getListTitlePluralString');
    return Promise.resolve('');
  }

  /**
   * Sets the list of incompatible applications returned by
   * requestIncompatibleApplicationsList().
   * @param {!Array<!settings.IncompatibleApplication>} incompatibleApplications
   */
  setIncompatibleApplications(incompatibleApplications) {
    this.incompatibleApplications_ = incompatibleApplications;
  }
}

suite('incompatibleApplicationsHandler', function() {
  let incompatibleApplicationsPage = null;

  /** @type {?TestIncompatibleApplicationsBrowserProxy} */
  let incompatibleApplicationsBrowserProxy = null;

  const incompatibleApplication1 = {
    'name': 'Application 1',
    'type': 0,
    'url': '',
  };
  const incompatibleApplication2 = {
    'name': 'Application 2',
    'type': 0,
    'url': '',
  };
  const incompatibleApplication3 = {
    'name': 'Application 3',
    'type': 0,
    'url': '',
  };
  const learnMoreIncompatibleApplication = {
    'name': 'Update Application',
    'type': 1,
    'url': 'chrome://update-url',
  };
  const updateIncompatibleApplication = {
    'name': 'Update Application',
    'type': 2,
    'url': 'chrome://update-url',
  };

  /**
   * @param {!Array<settings.IncompatibleApplication>}
   */
  function validateList(incompatibleApplications) {
    if (incompatibleApplications.length === 0) {
      const list =
          incompatibleApplicationsPage.$$('#incompatible-applications-list');
      assertEquals('none', list.style.display);
      // The contents of a dom-if that is false no longer receive updates in
      // Polymer 2. When there are no applications the parent dom-if becomes
      // false, so only check that the list is hidden, but don't assert on
      // number of DOM children.
      return;
    }

    const list = incompatibleApplicationsPage.shadowRoot.querySelectorAll(
        '.incompatible-application:not([hidden])');

    assertEquals(list.length, incompatibleApplications.length);
  }

  setup(function() {
    incompatibleApplicationsBrowserProxy =
        new TestIncompatibleApplicationsBrowserProxy();
    settings.IncompatibleApplicationsBrowserProxyImpl.instance_ =
        incompatibleApplicationsBrowserProxy;
  });

  /**
   * @param {boolean} hasAdminRights
   * @return {!Promise}
   */
  function initPage(hasAdminRights) {
    incompatibleApplicationsBrowserProxy.reset();
    PolymerTest.clearBody();

    loadTimeData.overrideValues({
      hasAdminRights: hasAdminRights,
    });

    incompatibleApplicationsPage =
        document.createElement('settings-incompatible-applications-page');
    document.body.appendChild(incompatibleApplicationsPage);
    return incompatibleApplicationsBrowserProxy
        .whenCalled('requestIncompatibleApplicationsList')
        .then(function() {
          Polymer.dom.flush();
        });
  }

  test('openMultipleIncompatibleApplications', function() {
    const multipleIncompatibleApplicationsTestList = [
      incompatibleApplication1,
      incompatibleApplication2,
      incompatibleApplication3,
    ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        multipleIncompatibleApplicationsTestList);

    return initPage(true).then(function() {
      validateList(multipleIncompatibleApplicationsTestList);
    });
  });

  test('startApplicationUninstallation', function() {
    const singleIncompatibleApplicationTestList = [
      incompatibleApplication1,
    ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        singleIncompatibleApplicationTestList);

    return initPage(true /* hasAdminRights */)
        .then(function() {
          validateList(singleIncompatibleApplicationTestList);

          // Retrieve the incompatible-application-item and tap it. It should be
          // visible.
          const item = incompatibleApplicationsPage.$$(
              '.incompatible-application:not([hidden])');
          item.$$('.action-button').click();

          return incompatibleApplicationsBrowserProxy.whenCalled(
              'startApplicationUninstallation');
        })
        .then(function(applicationName) {
          assertEquals(incompatibleApplication1.name, applicationName);
        });
  });

  test('learnMore', function() {
    const singleUpdateIncompatibleApplicationTestList = [
      learnMoreIncompatibleApplication,
    ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        singleUpdateIncompatibleApplicationTestList);

    return initPage(true /* hasAdminRights */)
        .then(function() {
          validateList(singleUpdateIncompatibleApplicationTestList);

          // Retrieve the incompatible-application-item and tap it. It should be
          // visible.
          const item = incompatibleApplicationsPage.$$(
              '.incompatible-application:not([hidden])');
          item.$$('.action-button').click();

          return incompatibleApplicationsBrowserProxy.whenCalled('openURL');
        })
        .then(function(url) {
          assertEquals(updateIncompatibleApplication.url, url);
        });
  });

  test('noAdminRights', function() {
    const eachTypeIncompatibleApplicationsTestList = [
      incompatibleApplication1,
      learnMoreIncompatibleApplication,
      updateIncompatibleApplication,
    ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        eachTypeIncompatibleApplicationsTestList);

    return initPage(false /* hasAdminRights */).then(function() {
      validateList(eachTypeIncompatibleApplicationsTestList);

      const items = incompatibleApplicationsPage.shadowRoot.querySelectorAll(
          '.incompatible-application:not([hidden])');

      assertEquals(items.length, 3);

      items.forEach(function(item, index) {
        // Just the name of the incompatible application is displayed inside a
        // div node. The <incompatible-application-item> component is not used.
        item.textContent.includes(
            eachTypeIncompatibleApplicationsTestList[index].name);
        assertNotEquals(item.nodeName, 'INCOMPATIBLE-APPLICATION-ITEM');
      });
    });
  });

  test('removeSingleApplication', function() {
    const incompatibleApplicationsTestList = [
      incompatibleApplication1,
    ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        incompatibleApplicationsTestList);

    return initPage(true /* hasAdminRights */).then(function() {
      validateList(incompatibleApplicationsTestList);


      const isDoneSection = incompatibleApplicationsPage.$$('#is-done-section');
      assertTrue(isDoneSection.hidden);

      // Send the event.
      cr.webUIListenerCallback(
          'incompatible-application-removed', incompatibleApplication1.name);
      Polymer.dom.flush();

      // Make sure the list is now empty.
      validateList([]);

      // The "Done!" text is visible.
      assertFalse(isDoneSection.hidden);
    });
  });
});
