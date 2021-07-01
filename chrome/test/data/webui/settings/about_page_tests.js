// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, isMac, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AboutPageBrowserProxyImpl, LifetimeBrowserProxyImpl, PromoteUpdaterStatus, Route, Router, UpdateStatus} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// clang-format on

function setupRouter() {
  const routes = {
    ABOUT: new Route('/help'),
    ADVANCED: new Route('/advanced'),
    BASIC: new Route('/'),
  };
  Router.resetInstanceForTesting(new Router(routes));
  return routes;
}

/**
 * @param {!UpdateStatus} status
 * @param {{
 *   progress: (number|undefined),
 *   message: (string|undefined)
 * }=} opt_options
 */
function fireStatusChanged(status, opt_options) {
  const options = opt_options || {};
  webUIListenerCallback('update-status-changed', {
    progress: options.progress === undefined ? 1 : options.progress,
    message: options.message,
    status: status,
  });
}

suite('AboutPageTest_AllBuilds', function() {
  /** @type {?SettingsAboutPageElement} */
  let page = null;

  /** @type {?TestAboutPageBrowserProxy} */
  let aboutBrowserProxy = null;

  /** @type {?TestLifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  /** @type {string} */
  const SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

  let testRoutes = null;

  setup(function() {
    loadTimeData.overrideValues({
      aboutObsoleteNowOrSoon: false,
      aboutObsoleteEndOfTheLine: false,
    });

    testRoutes = setupRouter();
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

    aboutBrowserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.instance_ = aboutBrowserProxy;
    return initNewPage();
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  /** @return {!Promise} */
  function initNewPage() {
    aboutBrowserProxy.reset();
    lifetimeBrowserProxy.reset();
    document.body.innerHTML = '';
    page = /** @type {!SettingsAboutPageElement} */ (
        document.createElement('settings-about-page'));
    Router.getInstance().navigateTo(testRoutes.ABOUT);
    document.body.appendChild(page);
    return isChromeOS ? Promise.resolve() :
                        aboutBrowserProxy.whenCalled('refreshUpdateStatus');
  }

  if (!isChromeOS) {
    /**
     * Test that the status icon and status message update according to
     * incoming 'update-status-changed' events.
     */
    test('IconAndMessageUpdates', function() {
      const icon = page.$$('iron-icon');
      assertTrue(!!icon);
      const statusMessageEl = page.$$('#updateStatusMessage div');
      let previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(SPINNER_ICON, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertNotEquals(previousMessageText, statusMessageEl.textContent);
      previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
      assertEquals(SPINNER_ICON, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertFalse(statusMessageEl.textContent.includes('%'));
      assertNotEquals(previousMessageText, statusMessageEl.textContent);
      previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
      assertNotEquals(previousMessageText, statusMessageEl.textContent);
      assertTrue(statusMessageEl.textContent.includes('%'));
      previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertEquals(null, icon.src);
      assertEquals('settings:check-circle', icon.icon);
      assertNotEquals(previousMessageText, statusMessageEl.textContent);
      previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
      assertEquals(null, icon.src);
      assertEquals('cr20:domain', icon.icon);
      assertEquals(0, statusMessageEl.textContent.trim().length);

      fireStatusChanged(UpdateStatus.FAILED);
      assertEquals(null, icon.src);
      assertEquals('cr:error', icon.icon);
      assertEquals(0, statusMessageEl.textContent.trim().length);

      fireStatusChanged(UpdateStatus.DISABLED);
      assertEquals(null, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertEquals(0, statusMessageEl.textContent.trim().length);
    });

    test('ErrorMessageWithHtml', function() {
      const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
      fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
      const statusMessageEl = page.$$('#updateStatusMessage div');
      assertEquals(htmlError, statusMessageEl.innerHTML);
    });

    test('FailedLearnMoreLink', function() {
      // Check that link is shown when update failed.
      fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
      assertTrue(!!page.$$('#updateStatusMessage a:not([hidden])'));

      // Check that link is hidden when update hasn't failed.
      fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
      assertTrue(!!page.$$('#updateStatusMessage a[hidden]'));
    });

    /**
     * Test that when the current platform has been marked as deprecated
     * (but not end of the line) a deprecation warning message is displayed,
     * without interfering with the update status message and icon.
     */
    test('ObsoleteSystem', async () => {
      loadTimeData.overrideValues({
        aboutObsoleteNowOrSoon: true,
        aboutObsoleteEndOfTheLine: false,
      });

      await initNewPage();
      const icon = page.$$('iron-icon');
      assertTrue(!!icon);
      assertTrue(!!page.$$('#updateStatusMessage'));
      assertTrue(!!page.$$('#deprecationWarning'));
      assertFalse(page.$$('#deprecationWarning').hidden);

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(SPINNER_ICON, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertFalse(page.$$('#updateStatusMessage').hidden);

      fireStatusChanged(UpdateStatus.UPDATING);
      assertEquals(SPINNER_ICON, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertFalse(page.$$('#updateStatusMessage').hidden);

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertEquals(null, icon.src);
      assertEquals('settings:check-circle', icon.icon);
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertFalse(page.$$('#updateStatusMessage').hidden);
    });

    /**
     * Test that when the current platform has reached the end of the line,
     * a deprecation warning message and an error icon is displayed.
     */
    test('ObsoleteSystemEndOfLine', async () => {
      loadTimeData.overrideValues({
        aboutObsoleteNowOrSoon: true,
        aboutObsoleteEndOfTheLine: true,
      });
      await initNewPage();
      const icon = page.$$('iron-icon');
      assertTrue(!!icon);
      assertTrue(!!page.$$('#deprecationWarning'));
      assertTrue(!!page.$$('#updateStatusMessage'));

      assertFalse(page.$$('#deprecationWarning').hidden);
      assertFalse(page.$$('#deprecationWarning').hidden);

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(null, icon.src);
      assertEquals('cr:error', icon.icon);
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertTrue(page.$$('#updateStatusMessage').hidden);

      fireStatusChanged(UpdateStatus.FAILED);
      assertEquals(null, icon.src);
      assertEquals('cr:error', icon.icon);
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertTrue(page.$$('#updateStatusMessage').hidden);

      fireStatusChanged(UpdateStatus.UPDATED);
      assertEquals(null, icon.src);
      assertEquals('cr:error', icon.icon);
      assertFalse(page.$$('#deprecationWarning').hidden);
      assertTrue(page.$$('#updateStatusMessage').hidden);
    });

    test('Relaunch', function() {
      let relaunch = page.$$('#relaunch');
      assertTrue(!!relaunch);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertFalse(relaunch.hidden);

      relaunch = page.$$('#relaunch');
      assertTrue(!!relaunch);
      relaunch.click();
      return lifetimeBrowserProxy.whenCalled('relaunch');
    });

    /*
     * Test that the "Relaunch" button updates according to incoming
     * 'update-status-changed' events.
     */
    test('ButtonsUpdate', function() {
      const relaunch = page.$$('#relaunch');
      assertTrue(!!relaunch);

      fireStatusChanged(UpdateStatus.CHECKING);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.UPDATING);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertFalse(relaunch.hidden);

      fireStatusChanged(UpdateStatus.UPDATED);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.FAILED);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.DISABLED);
      assertTrue(relaunch.hidden);

      fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
      assertTrue(relaunch.hidden);
    });
  }

  test('GetHelp', function() {
    assertTrue(!!page.$$('#help'));
    page.$$('#help').click();
    return aboutBrowserProxy.whenCalled('openHelpPage');
  });
});

suite('AboutPageTest_OfficialBuilds', function() {
  let page = null;
  let browserProxy = null;
  let testRoutes = null;

  setup(function() {
    testRoutes = setupRouter();
    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    page = document.createElement('settings-about-page');
    document.body.appendChild(page);
  });

  test('ReportAnIssue', function() {
    assertTrue(!!page.$$('#reportIssue'));
    page.$$('#reportIssue').click();
    return browserProxy.whenCalled('openFeedbackDialog');
  });

  if (isMac) {
    /**
     * A list of possible scenarios for the promoteUpdater.
     * @enum {!PromoteUpdaterStatus}
     */
    const PromoStatusScenarios = {
      CANT_PROMOTE: {
        hidden: true,
        disabled: true,
        actionable: false,
      },
      CAN_PROMOTE: {
        hidden: false,
        disabled: false,
        actionable: true,
      },
      IN_BETWEEN: {
        hidden: false,
        disabled: true,
        actionable: true,
      },
      PROMOTED: {
        hidden: false,
        disabled: true,
        actionable: false,
      },
    };

    /**
     * @param {!PromoteUpdaterStatus} status
     */
    function firePromoteUpdaterStatusChanged(status) {
      webUIListenerCallback('promotion-state-changed', status);
    }

    /**
     * Tests that the button's states are wired up to the status correctly.
     */
    test('PromoteUpdaterButtonCorrectStates', function() {
      let item = page.$$('#promoteUpdater');
      let arrow = page.$$('#promoteUpdater cr-icon-button');
      assertFalse(!!item);
      assertFalse(!!arrow);

      firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
      flush();
      item = page.$$('#promoteUpdater');
      arrow = page.$$('#promoteUpdater cr-icon-button');
      assertFalse(!!item);
      assertFalse(!!arrow);

      firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
      flush();

      item = page.$$('#promoteUpdater');
      assertTrue(!!item);
      assertFalse(item.hasAttribute('disabled'));
      assertTrue(item.hasAttribute('actionable'));

      arrow = page.$$('#promoteUpdater cr-icon-button');
      assertTrue(!!arrow);
      assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
      assertFalse(arrow.parentElement.hidden);
      assertFalse(arrow.hasAttribute('disabled'));

      firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
      flush();
      item = page.$$('#promoteUpdater');
      assertTrue(!!item);
      assertTrue(item.hasAttribute('disabled'));
      assertTrue(item.hasAttribute('actionable'));

      arrow = page.$$('#promoteUpdater cr-icon-button');
      assertTrue(!!arrow);
      assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
      assertFalse(arrow.parentElement.hidden);
      assertTrue(arrow.hasAttribute('disabled'));

      firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
      flush();
      item = page.$$('#promoteUpdater');
      assertTrue(!!item);
      assertTrue(item.hasAttribute('disabled'));
      assertFalse(item.hasAttribute('actionable'));

      arrow = page.$$('#promoteUpdater cr-icon-button');
      assertTrue(!!arrow);
      assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
      assertTrue(arrow.parentElement.hidden);
      assertTrue(arrow.hasAttribute('disabled'));
    });

    test('PromoteUpdaterButtonWorksWhenEnabled', function() {
      firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
      flush();
      const item = page.$$('#promoteUpdater');
      assertTrue(!!item);

      item.click();

      return browserProxy.whenCalled('promoteUpdater');
    });
  }
});
