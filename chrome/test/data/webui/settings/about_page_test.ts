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
      settingsRefresh2026: '',
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

  test('ProductRow_SettingsRefresh2026', async function() {
    // When the flag is disabled, row 1 contains the product title and logo,
    // and row 2 contains the icon container, all in a single main section.
    assertTrue(!!page.shadowRoot.querySelector('.product-title'));
    // <if expr="not is_chromeos">
    assertTrue(!!page.shadowRoot.querySelector('.icon-container'));
    // </if>
    const productLogo = page.shadowRoot.querySelector('#productLogo');
    assertTrue(!!productLogo);
    assertEquals(
        page.shadowRoot.querySelector('.product-title')!.parentElement,
        productLogo.parentElement);
    const initialSections =
        page.shadowRoot.querySelectorAll('settings-section');
    assertEquals(2, initialSections.length);
    assertTrue(initialSections[0]!.contains(productLogo));
    assertTrue(
        initialSections[0]!.contains(page.shadowRoot.querySelector('#help')));

    // When the flag is enabled, row 1 is removed and row 2 contains the logo
    // followed by the update text, without a separate icon container, in its
    // own section separate from the action rows below it.
    loadTimeData.overrideValues({
      settingsRefresh2026: 'settings-refresh-2026',
    });
    await initNewPage();

    assertFalse(!!page.shadowRoot.querySelector('.product-title'));
    // <if expr="not is_chromeos">
    assertFalse(!!page.shadowRoot.querySelector('.icon-container'));
    // </if>
    const refreshProductLogo =
        page.shadowRoot.querySelector<HTMLElement>('#productLogo');
    assertTrue(!!refreshProductLogo);
    const versionEl = page.shadowRoot.querySelector('.secondary');
    assertTrue(!!versionEl);
    const row = refreshProductLogo.parentElement!;
    assertEquals(row, versionEl.closest('.cr-row'));
    assertTrue(row.classList.contains('first'));
    assertTrue(row.classList.contains('two-line'));

    const refreshSections =
        page.shadowRoot.querySelectorAll('settings-section');
    assertEquals(3, refreshSections.length);
    assertTrue(refreshSections[0]!.contains(refreshProductLogo));
    assertFalse(
        refreshSections[0]!.contains(page.shadowRoot.querySelector('#help')));
    assertTrue(
        refreshSections[1]!.contains(page.shadowRoot.querySelector('#help')));

    // <if expr="not is_chromeos">
    const refreshThrobber =
        page.shadowRoot.querySelector<HTMLImageElement>('#throbber');
    assertTrue(!!refreshThrobber);
    assertFalse(refreshProductLogo.hidden);
    assertTrue(refreshThrobber.hidden);

    // When an update is in progress, the logo is replaced by the spinner.
    fireStatusChanged(UpdateStatus.UPDATING);
    await microtasksFinished();
    assertTrue(refreshProductLogo.hidden);
    assertFalse(refreshThrobber.hidden);

    // When update finishes, the logo is visible again and spinner is hidden.
    fireStatusChanged(UpdateStatus.UPDATED);
    await microtasksFinished();
    assertFalse(refreshProductLogo.hidden);
    assertTrue(refreshThrobber.hidden);
    // </if>
  });

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

  async function initOfficialBuildPage(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-about-page');
    Router.getInstance().navigateTo(routes.ABOUT);
    document.body.appendChild(page);
    await microtasksFinished();
  }

  setup(async function() {
    loadTimeData.overrideValues({
      settingsRefresh2026: '',
    });
    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    await initOfficialBuildPage();
  });

  test('ReportAnIssue', async function() {
    assertTrue(!!page.shadowRoot.querySelector('#reportIssue'));
    page.shadowRoot.querySelector<HTMLElement>('#reportIssue')!.click();
    await browserProxy.whenCalled('openFeedbackDialog');
  });

  test('PrivacyPolicy', async function() {
    const privacyPolicyLink =
        page.shadowRoot.querySelector<HTMLElement>('cr-link-row#privacyPolicy');
    assertTrue(!!privacyPolicyLink);
    privacyPolicyLink.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(ABOUT_PAGE_PRIVACY_POLICY_URL, url);
  });

  test('FooterLinks_SettingsRefresh2026', async function() {
    assertTrue(!!page.shadowRoot.querySelector('cr-link-row#privacyPolicy'));
    assertFalse(!!page.shadowRoot.querySelector('a#privacyPolicy'));

    loadTimeData.overrideValues({
      settingsRefresh2026: 'settings-refresh-2026',
    });
    await initOfficialBuildPage();

    assertFalse(!!page.shadowRoot.querySelector('cr-link-row#privacyPolicy'));
    const privacyPolicyEl =
        page.shadowRoot.querySelector<HTMLAnchorElement>('a#privacyPolicy');
    assertTrue(!!privacyPolicyEl);
    assertEquals(page.i18n('aboutPrivacyPolicy'), privacyPolicyEl.textContent);
    assertEquals(ABOUT_PAGE_PRIVACY_POLICY_URL, privacyPolicyEl.href);
    assertEquals('_blank', privacyPolicyEl.target);
    assertEquals(
        page.i18n('opensInNewTab'),
        privacyPolicyEl.getAttribute('aria-description'));

    const termsOfServiceEl =
        page.shadowRoot.querySelector<HTMLAnchorElement>('a#tos');
    assertTrue(!!termsOfServiceEl);
    assertEquals(page.i18n('aboutProductTos'), termsOfServiceEl.textContent);
    assertEquals(page.i18n('aboutTermsURL'), termsOfServiceEl.href);

    assertEquals(privacyPolicyEl.parentElement, termsOfServiceEl.parentElement);
    assertEquals(
        `${page.i18n('aboutPrivacyPolicy')} | ${page.i18n('aboutProductTos')}`,
        termsOfServiceEl.parentElement!.innerText);
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
