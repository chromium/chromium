// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {AboutPageBrowserProxyImpl, LifetimeBrowserProxyImpl, Route, Router, SettingsAboutPageElement, SettingsRoutes} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// <if expr="_google_chrome and is_macosx">
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PromoteUpdaterStatus} from 'chrome://settings/settings.js';
// </if>

// <if expr="not chromeos_ash">
import {UpdateStatus} from 'chrome://settings/settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertFalse, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

// clang-format on

function setupRouter(): SettingsRoutes {
  const routes = {
    ABOUT: new Route('/help'),
    ADVANCED: new Route('/advanced'),
    BASIC: new Route('/'),
  } as unknown as SettingsRoutes;
  Router.resetInstanceForTesting(new Router(routes));
  return routes;
}

// <if expr="not chromeos_ash">
function fireStatusChanged(
    status: UpdateStatus, options: {progress?: number, message?: string} = {}) {
  webUIListenerCallback('update-status-changed', {
    progress: options.progress === undefined ? 1 : options.progress,
    message: options.message,
    status: status,
  });
}
// </if>

suite('AboutPageTest_AllBuilds', function() {
  let page: SettingsAboutPageElement;
  let aboutBrowserProxy: TestAboutPageBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;

  let testRoutes: SettingsRoutes;

  setup(function() {
    loadTimeData.overrideValues({
      aboutObsoleteNowOrSoon: false,
      aboutObsoleteEndOfTheLine: false,
    });

    testRoutes = setupRouter();
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    aboutBrowserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstance(aboutBrowserProxy);
    return initNewPage();
  });

  teardown(function() {
    page.remove();
  });

  function initNewPage(): Promise<void> {
    aboutBrowserProxy.reset();
    lifetimeBrowserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-about-page');
    Router.getInstance().navigateTo(testRoutes.ABOUT);
    document.body.appendChild(page);
    // <if expr="chromeos_ash">
    return Promise.resolve();
    // </if>

    // <if expr="not chromeos_ash">
    return aboutBrowserProxy.whenCalled('refreshUpdateStatus');
    // </if>
  }

  // <if expr="not chromeos_ash">
  const SPINNER_ICON: string = 'chrome://resources/images/throbber_small.svg';

  /**
   * Test that the status icon and status message update according to
   * incoming 'update-status-changed' events.
   */
  test('IconAndMessageUpdates', function() {
    const icon = page.shadowRoot!.querySelector('iron-icon')!;
    assertTrue(!!icon);
    const statusMessageEl =
        page.shadowRoot!.querySelector('#updateStatusMessage div')!;
    let previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.CHECKING);
    assertEquals(SPINNER_ICON, icon.src);
    assertEquals(null, icon.getAttribute('icon'));
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
    assertEquals(SPINNER_ICON, icon.src);
    assertEquals(null, icon.getAttribute('icon'));
    assertFalse(statusMessageEl.textContent!.includes('%'));
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    assertTrue(statusMessageEl.textContent!.includes('%'));
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertEquals(null, icon.src);
    assertEquals('settings:check-circle', icon.icon);
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
    assertEquals(null, icon.src);
    assertEquals('cr20:domain', icon.icon);
    assertEquals(0, statusMessageEl.textContent!.trim().length);

    fireStatusChanged(UpdateStatus.FAILED);
    assertEquals(null, icon.src);
    assertEquals('cr:error', icon.icon);
    assertEquals(0, statusMessageEl.textContent!.trim().length);

    fireStatusChanged(UpdateStatus.DISABLED);
    assertEquals(null, icon.src);
    assertEquals(null, icon.getAttribute('icon'));
    assertEquals(0, statusMessageEl.textContent!.trim().length);
  });

  test('ErrorMessageWithHtml', function() {
    const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
    fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
    const statusMessageEl =
        page.shadowRoot!.querySelector('#updateStatusMessage div');
    assertEquals(htmlError, statusMessageEl!.innerHTML);
  });

  test('FailedLearnMoreLink', function() {
    // Check that link is shown when update failed.
    fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
    assertTrue(!!page.shadowRoot!.querySelector(
        '#updateStatusMessage a:not([hidden])'));

    // Check that link is hidden when update hasn't failed.
    fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
    assertTrue(
        !!page.shadowRoot!.querySelector('#updateStatusMessage a[hidden]'));
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

    function queryDeprecationWarning() {
      return page.shadowRoot!.querySelector<HTMLElement>('#deprecationWarning')!
          ;
    }

    function queryUpdateStatusMessage() {
      return page.shadowRoot!.querySelector<HTMLElement>(
          '#updateStatusMessage')!;
    }

    await initNewPage();
    const icon = page.shadowRoot!.querySelector('iron-icon')!;
    assertTrue(!!icon);
    assertTrue(!!queryUpdateStatusMessage());
    assertTrue(!!queryDeprecationWarning());
    assertFalse(queryDeprecationWarning().hidden);

    fireStatusChanged(UpdateStatus.CHECKING);
    assertEquals(SPINNER_ICON, icon.src);
    assertEquals(null, icon.getAttribute('icon'));
    assertFalse(queryDeprecationWarning().hidden);
    assertFalse(queryUpdateStatusMessage().hidden);

    fireStatusChanged(UpdateStatus.UPDATING);
    assertEquals(SPINNER_ICON, icon.src);
    assertEquals(null, icon.getAttribute('icon'));
    assertFalse(queryDeprecationWarning().hidden);
    assertFalse(queryUpdateStatusMessage().hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertEquals(null, icon.src);
    assertEquals('settings:check-circle', icon.icon);
    assertFalse(queryDeprecationWarning().hidden);
    assertFalse(queryUpdateStatusMessage().hidden);
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

    function queryDeprecationWarning() {
      return page.shadowRoot!.querySelector<HTMLElement>('#deprecationWarning')!
          ;
    }

    function queryUpdateStatusMessage() {
      return page.shadowRoot!.querySelector<HTMLElement>(
          '#updateStatusMessage')!;
    }

    await initNewPage();
    const icon = page.shadowRoot!.querySelector('iron-icon')!;
    assertTrue(!!icon);
    assertTrue(!!queryDeprecationWarning());
    assertTrue(!!queryUpdateStatusMessage());

    assertFalse(queryDeprecationWarning().hidden);
    assertTrue(queryUpdateStatusMessage().hidden);

    fireStatusChanged(UpdateStatus.CHECKING);
    assertEquals(null, icon.src);
    assertEquals('cr:error', icon.icon);
    assertFalse(queryDeprecationWarning().hidden);
    assertTrue(queryUpdateStatusMessage().hidden);

    fireStatusChanged(UpdateStatus.FAILED);
    assertEquals(null, icon.src);
    assertEquals('cr:error', icon.icon);
    assertFalse(queryDeprecationWarning().hidden);
    assertTrue(queryUpdateStatusMessage().hidden);

    fireStatusChanged(UpdateStatus.UPDATED);
    assertEquals(null, icon.src);
    assertEquals('cr:error', icon.icon);
    assertFalse(queryDeprecationWarning().hidden);
    assertTrue(queryUpdateStatusMessage().hidden);
  });

  test('Relaunch', function() {
    let relaunch = page.shadowRoot!.querySelector<HTMLElement>('#relaunch')!;
    assertTrue(!!relaunch);
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertFalse(relaunch.hidden);

    relaunch = page.shadowRoot!.querySelector<HTMLElement>('#relaunch')!;
    assertTrue(!!relaunch);
    relaunch.click();
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  /*
   * Test that the "Relaunch" button updates according to incoming
   * 'update-status-changed' events.
   */
  test('ButtonsUpdate', function() {
    const relaunch = page.shadowRoot!.querySelector<HTMLElement>('#relaunch')!;
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
  // </if>

  test('GetHelp', function() {
    assertTrue(!!page.shadowRoot!.querySelector('#help'));
    page.shadowRoot!.querySelector<HTMLElement>('#help')!.click();
    return aboutBrowserProxy.whenCalled('openHelpPage');
  });
});

suite('AboutPageTest_OfficialBuilds', function() {
  let page: SettingsAboutPageElement;
  let browserProxy: TestAboutPageBrowserProxy;

  setup(function() {
    setupRouter();
    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-about-page');
    document.body.appendChild(page);
  });

  test('ReportAnIssue', async function() {
    assertTrue(!!page.shadowRoot!.querySelector('#reportIssue'));
    page.shadowRoot!.querySelector<HTMLElement>('#reportIssue')!.click();
    await browserProxy.whenCalled('openFeedbackDialog');
  });

  // <if expr="_google_chrome and is_macosx">
  type Scenarios = 'CANT_PROMOTE'|'CAN_PROMOTE'|'IN_BETWEEN'|'PROMOTED';

  /**
   * A list of possible scenarios for the promoteUpdater.
   */
  const PromoStatusScenarios: {[key in Scenarios]: PromoteUpdaterStatus} = {
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

  function firePromoteUpdaterStatusChanged(status: PromoteUpdaterStatus) {
    webUIListenerCallback('promotion-state-changed', status);
  }

  /**
   * Tests that the button's states are wired up to the status correctly.
   */
  test('PromoteUpdaterButtonCorrectStates', function() {
    function queryPromoteUpdater() {
      return page.shadowRoot!.querySelector<HTMLElement>('#promoteUpdater');
    }

    function queryArrowIcon() {
      return page.shadowRoot!.querySelector<HTMLElement>(
          '#promoteUpdater cr-icon-button');
    }

    let item = queryPromoteUpdater();
    let arrow = queryArrowIcon();
    assertFalse(!!item);
    assertFalse(!!arrow);

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
    flush();
    item = queryPromoteUpdater();
    arrow = queryArrowIcon();
    assertFalse(!!item);
    assertFalse(!!arrow);

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
    flush();

    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertFalse(item!.hasAttribute('disabled'));
    assertTrue(item!.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow!.parentElement!.tagName);
    assertFalse(arrow!.parentElement!.hidden);
    assertFalse(arrow!.hasAttribute('disabled'));

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
    flush();
    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertTrue(item!.hasAttribute('disabled'));
    assertTrue(item!.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow!.parentElement!.tagName);
    assertFalse(arrow!.parentElement!.hidden);
    assertTrue(arrow!.hasAttribute('disabled'));

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
    flush();
    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertTrue(item!.hasAttribute('disabled'));
    assertFalse(item!.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow!.parentElement!.tagName);
    assertTrue(arrow!.parentElement!.hidden);
    assertTrue(arrow!.hasAttribute('disabled'));
  });

  test('PromoteUpdaterButtonWorksWhenEnabled', async function() {
    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
    flush();
    const item = page.shadowRoot!.querySelector<HTMLElement>('#promoteUpdater');
    assertTrue(!!item);

    item!.click();

    await browserProxy.whenCalled('promoteUpdater');
  });
  // </if>
});
