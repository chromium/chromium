// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/welcome_app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {LandingViewProxyImpl} from 'chrome://welcome/landing_view_proxy.js';
import {navigateTo, Routes} from 'chrome://welcome/router.js';
import {NuxSetAsDefaultProxyImpl} from 'chrome://welcome/set_as_default/nux_set_as_default_proxy.js';
import {BookmarkProxyImpl} from 'chrome://welcome/shared/bookmark_proxy.js';
import type {DefaultBrowserInfo} from 'chrome://welcome/shared/nux_types.js';
import type {WelcomeAppElement} from 'chrome://welcome/welcome_app.js';
import {WelcomeBrowserProxyImpl} from 'chrome://welcome/welcome_browser_proxy.js';

import {TestBookmarkProxy} from './test_bookmark_proxy.js';
import {TestLandingViewProxy} from './test_landing_view_proxy.js';
import {TestNuxSetAsDefaultProxy} from './test_nux_set_as_default_proxy.js';
import {TestWelcomeBrowserProxy} from './test_welcome_browser_proxy.js';

suite('WelcomeWelcomeAppTest', function() {
  let testElement: WelcomeAppElement;
  let testWelcomeBrowserProxy: TestWelcomeBrowserProxy;
  let testSetAsDefaultProxy: TestNuxSetAsDefaultProxy;

  function resetTestElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigateTo(Routes.LANDING, 0);
    testElement = document.createElement('welcome-app');
    document.body.appendChild(testElement);
  }

  function simulateCanSetDefault() {
    testSetAsDefaultProxy.setDefaultStatus({
      isDefault: false,
      canBeDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    resetTestElement();
  }

  function simulateCannotSetDefault() {
    testSetAsDefaultProxy.setDefaultStatus({
      isDefault: true,
      canBeDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    resetTestElement();
  }

  setup(function() {
    testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
    WelcomeBrowserProxyImpl.setInstance(testWelcomeBrowserProxy);

    testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
    NuxSetAsDefaultProxyImpl.setInstance(testSetAsDefaultProxy);

    // Not used in test, but setting to test proxy anyway, in order to prevent
    // calls to backend.
    BookmarkProxyImpl.setInstance(new TestBookmarkProxy());
    LandingViewProxyImpl.setInstance(new TestLandingViewProxy());

    return resetTestElement();
  });

  teardown(function() {
    testElement.remove();
  });

  test('shows landing page by default', function() {
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const landingView = testElement.shadowRoot!.querySelector('landing-view');
    assertTrue(!!landingView);
    assertTrue(landingView!.classList.contains('active'));
  });

  test('new user route (can set default)', function() {
    simulateCanSetDefault();
    navigateTo(Routes.NEW_USER, 1);
    return waitBeforeNextRender(testElement).then(() => {
      const views = testElement.shadowRoot!.querySelectorAll('[slot=view]');
      assertEquals(views.length, 5);
      ['LANDING-VIEW',
       'NUX-GOOGLE-APPS',
       'NUX-NTP-BACKGROUND',
       'NUX-SET-AS-DEFAULT',
       'SIGNIN-VIEW',
      ].forEach((expectedView, ix) => {
        assertEquals(expectedView, views[ix]!.tagName);
      });
    });
  });

  test('new user route (cannot set default)', function() {
    simulateCannotSetDefault();
    navigateTo(Routes.NEW_USER, 1);
    return waitBeforeNextRender(testElement).then(() => {
      const views = testElement.shadowRoot!.querySelectorAll('[slot=view]');
      assertEquals(views.length, 4);
      ['LANDING-VIEW',
       'NUX-GOOGLE-APPS',
       'NUX-NTP-BACKGROUND',
       'SIGNIN-VIEW',
      ].forEach((expectedView, ix) => {
        assertEquals(expectedView, views[ix]!.tagName);
      });
    });
  });

  test('returning user route (can set default)', function() {
    simulateCanSetDefault();
    navigateTo(Routes.RETURNING_USER, 1);
    return waitBeforeNextRender(testElement).then(() => {
      const views = testElement.shadowRoot!.querySelectorAll('[slot=view]');
      assertEquals(views.length, 2);
      assertEquals(views[0]!.tagName, 'LANDING-VIEW');
      assertEquals(views[1]!.tagName, 'NUX-SET-AS-DEFAULT');
    });
  });

  test('returning user route (cannot set default)', function() {
    simulateCannotSetDefault();
    navigateTo(Routes.RETURNING_USER, 1);

    // At this point, there should be no steps in the returning user, so
    // welcome_app should try to go to NTP.
    return testWelcomeBrowserProxy.whenCalled('goToNewTabPage');
  });

  test('default-status check resolves with correct value', function() {
    function checkDefaultStatusResolveValue(
        status: DefaultBrowserInfo,
        expectedDefaultExists: boolean): Promise<void> {
      testSetAsDefaultProxy.setDefaultStatus(status);
      resetTestElement();

      // Use the new-user route to test if nux-set-as-default module gets
      // initialized.
      navigateTo(Routes.NEW_USER, 1);
      return waitBeforeNextRender(testElement).then(() => {
        // Use the existence of the nux-set-as-default as indication of
        // whether or not the promise is resolved with the expected result.
        assertEquals(
            expectedDefaultExists,
            !!testElement.shadowRoot!.querySelector('nux-set-as-default'));
      });
    }

    return checkDefaultStatusResolveValue(
               {
                 // Allowed to set as default, and not default yet.
                 isDefault: false,
                 canBeDefault: true,
                 isDisabledByPolicy: false,
                 isUnknownError: false,
               },
               true)
        .then(() => {
          return checkDefaultStatusResolveValue(
              {
                // Allowed to set as default, but already default.
                isDefault: true,
                canBeDefault: true,
                isDisabledByPolicy: false,
                isUnknownError: false,
              },
              false);
        })
        .then(() => {
          return checkDefaultStatusResolveValue(
              {
                // Not default yet, but this chrome install cannot be default.
                isDefault: false,
                canBeDefault: false,
                isDisabledByPolicy: false,
                isUnknownError: false,
              },
              false);
        })
        .then(() => {
          return checkDefaultStatusResolveValue(
              {
                // Not default yet, but setting default is disabled by policy.
                isDefault: false,
                canBeDefault: true,
                isDisabledByPolicy: true,
                isUnknownError: false,
              },
              false);
        })
        .then(() => {
          return checkDefaultStatusResolveValue(
              {
                // Not default yet, but there is some unknown error.
                isDefault: false,
                canBeDefault: true,
                isDisabledByPolicy: false,
                isUnknownError: true,
              },
              false);
        });
  });
});
