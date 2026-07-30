// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsAboutPageElement} from 'chrome://settings/settings.js';
import {AboutPageBrowserProxyImpl, LifetimeBrowserProxyImpl, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// <if expr="_google_chrome">
import {ABOUT_PAGE_PRIVACY_POLICY_URL, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
// </if>

// <if expr="_google_chrome and is_macosx">
import type {PromoteUpdaterStatus} from 'chrome://settings/settings.js';
// </if>

// <if expr="not is_chromeos">
import {UpdateStatus} from 'chrome://settings/settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {isVisible, eventToPromise} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

// <if expr="not is_chromeos">
function fireStatusChanged(
    status: UpdateStatus, options: {progress?: number, message?: string} = {}) {
  webUIListenerCallback('update-status-changed', {
    progress: options.progress === undefined ? 1 : options.progress,
    message: options.message,
    status: status,
  });
}
// </if>

suite('AllBuilds', function() {
  let page: SettingsAboutPageElement;
  let aboutBrowserProxy: TestAboutPageBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;

  setup(function() {
    loadTimeData.overrideValues({
      aboutObsoleteNowOrSoon: false,
      aboutObsoleteEndOfTheLine: false,
    });

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    aboutBrowserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstance(aboutBrowserProxy);
    return initNewPage();
  });

  teardown(function() {
    page.remove();
  });

  async function initNewPage(): Promise<void> {
    aboutBrowserProxy.reset();
    lifetimeBrowserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-about-page');
    Router.getInstance().navigateTo(routes.ABOUT);
    document.body.appendChild(page);
    await microtasksFinished();
    // <if expr="is_chromeos">
    return;
    // </if>

    // <if expr="not is_chromeos">
    await aboutBrowserProxy.whenCalled('refreshUpdateStatus');
    await microtasksFinished();
    // </if>
  }

  // <if expr="not is_chromeos">
  const SPINNER_ICON: string = 'chrome://resources/images/throbber_small.svg';

  async function assertSpinnerVisible(visible: boolean) {
    const img = page.shadowRoot.querySelector<HTMLImageElement>(
        `img[src='${SPINNER_ICON}']`);
    assertTrue(!!img);
    if (img.complete) {
      assertEquals(visible, isVisible(img));
      return;
    }

    await eventToPromise('load', img);
    assertEquals(visible, isVisible(img));
  }

  /**
   * Test that the status icon and status message update according to
   * incoming 'update-status-changed' events.
   */
  test('IconAndMessageUpdates', async function() {
    const icon = page.shadowRoot.querySelector('cr-icon')!;
    assertTrue(!!icon);
    const statusMessageEl =
        page.shadowRoot.querySelector('#updateStatusMessage div')!;
    let previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.CHECKING);
    await microtasksFinished();
    await assertSpinnerVisible(true);
    assertEquals('', icon.getAttribute('icon'));
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
    await microtasksFinished();
    await assertSpinnerVisible(true);
    assertEquals('', icon.getAttribute('icon'));
    assertFalse(statusMessageEl.textContent.includes('%'));
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
    await microtasksFinished();
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    assertTrue(statusMessageEl.textContent.includes('%'));
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:check-circle', icon.icon);
    assertNotEquals(previousMessageText, statusMessageEl.textContent);
    previousMessageText = statusMessageEl.textContent;

    fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr20:domain', icon.icon);
    assertEquals(0, statusMessageEl.textContent.trim().length);

    fireStatusChanged(UpdateStatus.FAILED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:error-filled', icon.icon);
    assertEquals(0, statusMessageEl.textContent.trim().length);

    fireStatusChanged(UpdateStatus.DISABLED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('', icon.getAttribute('icon'));
    assertEquals(0, statusMessageEl.textContent.trim().length);
  });

  test('ErrorMessageWithHtml', async function() {
    const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
    fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
    await microtasksFinished();
    const statusMessageEl =
        page.shadowRoot.querySelector('#updateStatusMessage div');
    assertEquals(htmlError, statusMessageEl!.innerHTML);
  });

  test('FailedLearnMoreLink', async function() {
    // Check that link is shown when update failed.
    fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
    await microtasksFinished();
    assertTrue(!!page.shadowRoot.querySelector(
        '#updateStatusMessage a:not([hidden])'));

    // Check that link is hidden when update hasn't failed.
    fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
    await microtasksFinished();
    assertTrue(
        !!page.shadowRoot.querySelector('#updateStatusMessage a[hidden]'));
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
    const icon = page.shadowRoot.querySelector('cr-icon')!;
    assertTrue(!!icon);
    assertTrue(!!page.$.updateStatusMessage);
    assertTrue(!!page.$.deprecationWarning);
    assertFalse(page.$.deprecationWarning.hidden);

    fireStatusChanged(UpdateStatus.CHECKING);
    await microtasksFinished();
    await assertSpinnerVisible(true);
    assertEquals('', icon.getAttribute('icon'));
    assertFalse(page.$.deprecationWarning.hidden);
    assertFalse(page.$.updateStatusMessage.hidden);

    fireStatusChanged(UpdateStatus.UPDATING);
    await microtasksFinished();
    await assertSpinnerVisible(true);
    assertEquals('', icon.getAttribute('icon'));
    assertFalse(page.$.deprecationWarning.hidden);
    assertFalse(page.$.updateStatusMessage.hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:check-circle', icon.icon);
    assertFalse(page.$.deprecationWarning.hidden);
    assertFalse(page.$.updateStatusMessage.hidden);
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
    const icon = page.shadowRoot.querySelector('cr-icon')!;
    assertTrue(!!icon);
    assertTrue(!!page.$.deprecationWarning);
    assertTrue(!!page.$.updateStatusMessage);

    assertFalse(page.$.deprecationWarning.hidden);
    assertTrue(page.$.updateStatusMessage.hidden);

    fireStatusChanged(UpdateStatus.CHECKING);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:error-filled', icon.icon);
    assertFalse(page.$.deprecationWarning.hidden);
    assertTrue(page.$.updateStatusMessage.hidden);

    fireStatusChanged(UpdateStatus.FAILED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:error-filled', icon.icon);
    assertFalse(page.$.deprecationWarning.hidden);
    assertTrue(page.$.updateStatusMessage.hidden);

    fireStatusChanged(UpdateStatus.UPDATED);
    await microtasksFinished();
    await assertSpinnerVisible(false);
    assertEquals('cr:error-filled', icon.icon);
    assertFalse(page.$.deprecationWarning.hidden);
    assertTrue(page.$.updateStatusMessage.hidden);
  });

  test('Relaunch', async function() {
    let relaunch = page.shadowRoot.querySelector<HTMLElement>('#relaunch')!;
    assertTrue(!!relaunch);
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    await microtasksFinished();
    assertFalse(relaunch.hidden);

    relaunch = page.shadowRoot.querySelector<HTMLElement>('#relaunch')!;
    assertTrue(!!relaunch);
    relaunch.click();
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  /*
   * Test that the "Relaunch" button updates according to incoming
   * 'update-status-changed' events.
   */
  test('ButtonsUpdate', async function() {
    const relaunch = page.shadowRoot.querySelector<HTMLElement>('#relaunch')!;
    assertTrue(!!relaunch);

    fireStatusChanged(UpdateStatus.CHECKING);
    await microtasksFinished();
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.UPDATING);
    await microtasksFinished();
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    await microtasksFinished();
    assertFalse(relaunch.hidden);

    fireStatusChanged(UpdateStatus.UPDATED);
    await microtasksFinished();
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.FAILED);
    await microtasksFinished();
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.DISABLED);
    await microtasksFinished();
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
    await microtasksFinished();
    assertTrue(relaunch.hidden);
  });

  // <if expr="_google_chrome or _is_chrome_for_testing_branded">
  test('TermsOfService', function() {
    const termsOfServiceEl =
        page.shadowRoot.querySelector<HTMLAnchorElement>('a#tos');
    assertTrue(!!termsOfServiceEl);

    assertEquals(page.i18n('aboutProductTos'), termsOfServiceEl.textContent);
    assertEquals(page.i18n('aboutTermsURL'), termsOfServiceEl.href);
  });
  // </if>

  // </if>
  test('GetHelp', function() {
    assertTrue(!!page.shadowRoot.querySelector('#help'));
    page.shadowRoot.querySelector<HTMLElement>('#help')!.click();
    return aboutBrowserProxy.whenCalled('openHelpPage');
  });

  test('searchContents', async function() {
    let result = await page.searchContents('foo');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await page.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
  });
});

// <if expr="_google_chrome">
suite('OfficialBuild', function() {
  let page: SettingsAboutPageElement;
  let browserProxy: TestAboutPageBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  setup(async function() {
    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-about-page');
    Router.getInstance().navigateTo(routes.ABOUT);
    document.body.appendChild(page);
    await microtasksFinished();
  });

  test('ReportAnIssue', async function() {
    assertTrue(!!page.shadowRoot.querySelector('#reportIssue'));
    page.shadowRoot.querySelector<HTMLElement>('#reportIssue')!.click();
    await browserProxy.whenCalled('openFeedbackDialog');
  });

  test('PrivacyPolicy', async function() {
    const privacyPolicyLink =
        page.shadowRoot.querySelector<HTMLElement>('#privacyPolicy');
    assertTrue(!!privacyPolicyLink);
    privacyPolicyLink.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(ABOUT_PAGE_PRIVACY_POLICY_URL, url);
  });

  // <if expr="is_macosx">
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
  test('PromoteUpdaterButtonCorrectStates', async function() {
    function queryPromoteUpdater() {
      return page.shadowRoot.querySelector<HTMLElement>('#promoteUpdater');
    }

    function queryArrowIcon() {
      return page.shadowRoot.querySelector<HTMLElement>(
          '#promoteUpdater cr-icon-button');
    }

    let item = queryPromoteUpdater();
    let arrow = queryArrowIcon();
    assertFalse(!!item);
    assertFalse(!!arrow);

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
    await microtasksFinished();
    item = queryPromoteUpdater();
    arrow = queryArrowIcon();
    assertFalse(!!item);
    assertFalse(!!arrow);

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
    await microtasksFinished();

    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertFalse(item.hasAttribute('disabled'));
    assertTrue(item.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow.parentElement!.tagName);
    assertFalse(arrow.parentElement!.hidden);
    assertFalse(arrow.hasAttribute('disabled'));

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
    await microtasksFinished();
    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertTrue(item.hasAttribute('disabled'));
    assertTrue(item.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow.parentElement!.tagName);
    assertFalse(arrow.parentElement!.hidden);
    assertTrue(arrow.hasAttribute('disabled'));

    firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
    await microtasksFinished();
    item = queryPromoteUpdater();
    assertTrue(!!item);
    assertTrue(item.hasAttribute('disabled'));
    assertFalse(item.hasAttribute('actionable'));

    arrow = queryArrowIcon();
    assertTrue(!!arrow);
    assertEquals('CR-ICON-BUTTON', arrow.parentElement!.tagName);
    assertTrue(arrow.parentElement!.hidden);
    assertTrue(arrow.hasAttribute('disabled'));
  });

  test('PromoteUpdaterButtonWorksWhenEnabled', async function() {
    firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
    await microtasksFinished();
    const item = page.shadowRoot.querySelector<HTMLElement>('#promoteUpdater');
    assertTrue(!!item);

    item.click();

    await browserProxy.whenCalled('promoteUpdater');
  });
  // </if>
});
// </if>
