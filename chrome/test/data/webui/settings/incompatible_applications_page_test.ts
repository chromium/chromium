// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ActionTypes, IncompatibleApplication, IncompatibleApplicationItemElement, IncompatibleApplicationsBrowserProxy, IncompatibleApplicationsBrowserProxyImpl, SettingsIncompatibleApplicationsPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

class TestIncompatibleApplicationsBrowserProxy extends TestBrowserProxy
    implements IncompatibleApplicationsBrowserProxy {
  private incompatibleApplications_: IncompatibleApplication[] = [];

  constructor() {
    super([
      'requestIncompatibleApplicationsList',
      'startApplicationUninstallation',
      'openURL',
      'getSubtitlePluralString',
      'getSubtitleNoAdminRightsPluralString',
      'getListTitlePluralString',
    ]);
  }

  requestIncompatibleApplicationsList() {
    this.methodCalled('requestIncompatibleApplicationsList');
    return Promise.resolve(this.incompatibleApplications_);
  }

  startApplicationUninstallation(applicationName: string) {
    this.methodCalled('startApplicationUninstallation', applicationName);
  }

  openURL(url: string) {
    this.methodCalled('openURL', url);
  }

  getSubtitlePluralString(numApplications: number) {
    this.methodCalled('getSubtitlePluralString', numApplications);
    return Promise.resolve('');
  }

  getSubtitleNoAdminRightsPluralString(numApplications: number) {
    this.methodCalled('getSubtitleNoAdminRightsPluralString', numApplications);
    return Promise.resolve('');
  }

  getListTitlePluralString(numApplications: number) {
    this.methodCalled('getListTitlePluralString', numApplications);
    return Promise.resolve('');
  }

  /**
   * Sets the list of incompatible applications returned by
   * requestIncompatibleApplicationsList().
   */
  setIncompatibleApplications(incompatibleApplications:
                                  IncompatibleApplication[]) {
    this.incompatibleApplications_ = incompatibleApplications;
  }
}

suite('incompatibleApplicationsHandler', function() {
  let incompatibleApplicationsPage: SettingsIncompatibleApplicationsPageElement;
  let incompatibleApplicationsBrowserProxy:
      TestIncompatibleApplicationsBrowserProxy;

  const incompatibleApplication1 = {
    name: 'Application 1',
    type: ActionTypes.UNINSTALL,
    url: '',
  };
  const incompatibleApplication2 = {
    name: 'Application 2',
    type: ActionTypes.UNINSTALL,
    url: '',
  };
  const incompatibleApplication3 = {
    name: 'Application 3',
    type: ActionTypes.UNINSTALL,
    url: '',
  };
  const learnMoreIncompatibleApplication = {
    name: 'Update Application',
    type: ActionTypes.MORE_INFO,
    url: 'chrome://update-url',
  };
  const updateIncompatibleApplication = {
    name: 'Update Application',
    type: ActionTypes.UPGRADE,
    url: 'chrome://update-url',
  };

  function validateList(incompatibleApplications: IncompatibleApplication[]) {
    if (incompatibleApplications.length === 0) {
      const list =
          incompatibleApplicationsPage.shadowRoot!.querySelector<HTMLElement>(
              '#incompatible-applications-list')!;
      assertEquals('none', list.style.display);
      // The contents of a dom-if that is false no longer receive updates. When
      // there are no applications the parent dom-if becomes false, so only
      // check that the list is hidden, but don't assert on number of DOM
      // children.
      return;
    }

    const list = incompatibleApplicationsPage.shadowRoot!.querySelectorAll(
        '.incompatible-application:not([hidden])');

    assertEquals(list.length, incompatibleApplications.length);
  }

  setup(function() {
    incompatibleApplicationsBrowserProxy =
        new TestIncompatibleApplicationsBrowserProxy();
    IncompatibleApplicationsBrowserProxyImpl.setInstance(
        incompatibleApplicationsBrowserProxy);
  });

  function initPage(hasAdminRights: boolean): Promise<void> {
    incompatibleApplicationsBrowserProxy.reset();
    document.body.innerHTML = '';

    loadTimeData.overrideValues({
      hasAdminRights: hasAdminRights,
    });

    incompatibleApplicationsPage =
        document.createElement('settings-incompatible-applications-page');
    document.body.appendChild(incompatibleApplicationsPage);
    return incompatibleApplicationsBrowserProxy
        .whenCalled('requestIncompatibleApplicationsList')
        .then(function() {
          flush();
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
          const item = incompatibleApplicationsPage.shadowRoot!
                           .querySelector<IncompatibleApplicationItemElement>(
                               '.incompatible-application:not([hidden])')!;
          item.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();

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
          const item = incompatibleApplicationsPage.shadowRoot!
                           .querySelector<IncompatibleApplicationItemElement>(
                               '.incompatible-application:not([hidden])')!;
          item.shadowRoot!.querySelector<HTMLElement>(
                              '.action-button')!.click();

          return incompatibleApplicationsBrowserProxy.whenCalled('openURL');
        })
        .then(function(url) {
          assertEquals(updateIncompatibleApplication.url, url);
        });
  });

  test('noAdminRights', function() {
    const eachTypeIncompatibleApplicationsTestList: IncompatibleApplication[] =
        [
          incompatibleApplication1,
          learnMoreIncompatibleApplication,
          updateIncompatibleApplication,
        ];

    incompatibleApplicationsBrowserProxy.setIncompatibleApplications(
        eachTypeIncompatibleApplicationsTestList);

    return initPage(false /* hasAdminRights */).then(function() {
      validateList(eachTypeIncompatibleApplicationsTestList);

      const items = incompatibleApplicationsPage.shadowRoot!.querySelectorAll(
          '.incompatible-application:not([hidden])');

      assertEquals(items.length, 3);

      items.forEach(function(item, index) {
        // Just the name of the incompatible application is displayed inside a
        // div node. The <incompatible-application-item> component is not used.
        item.textContent!.includes(
            eachTypeIncompatibleApplicationsTestList[index]!.name);
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


      const isDoneSection =
          incompatibleApplicationsPage.shadowRoot!.querySelector<HTMLElement>(
              '#is-done-section')!;
      assertTrue(isDoneSection.hidden);

      // Send the event.
      webUIListenerCallback(
          'incompatible-application-removed', incompatibleApplication1.name);
      flush();

      // Make sure the list is now empty.
      validateList([]);

      // The "Done!" text is visible.
      assertFalse(isDoneSection.hidden);
    });
  });
});
