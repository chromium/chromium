// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPrivacyGuidePageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, CookieControlsMode, PrivacyGuideStep, SafeBrowsingSetting, ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, PrefsBrowserProxy, Router, routes, SignedInState, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getSyncAllPrefs} from './sync_test_util.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import type {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

export function getInitialPrivacyGuideTestPrefs() {
  return [
    {
      key: 'privacy_guide.viewed',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'generated.cookie_default_content_setting',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: ContentSetting.ALLOW,
    },
    {
      key: 'profile.cookie_controls_mode',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: CookieControlsMode.INCOGNITO_ONLY,
    },
    {
      key: 'generated.third_party_cookie_blocking_setting',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY,
    },
    {
      key: 'generated.safe_browsing',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SafeBrowsingSetting.STANDARD,
    },
    {
      key: 'url_keyed_anonymized_data_collection.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'net.network_prediction_options',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    },
  ];
}

export function setupPrivacyRouteForTest(): void {
  // Simulates the route of the user entering the privacy guide from the S&P
  // settings. This is necessary as tests seem to by default define the
  // previous route as Settings "/". On a back navigation, "/" matches the
  // criteria for a valid Settings parent no matter how deep the subpage is in
  // the Settings tree. This would always navigate to Settings "/" instead of
  // to the parent of the current subpage.
  Router.getInstance().navigateTo(routes.PRIVACY);
}

/**
 * Equivalent of the user manually navigating to the corresponding step via
 * typing the URL and step parameter in the Omnibox.
 */
export function navigateToStep(step: PrivacyGuideStep): Promise<void> {
  Router.getInstance().navigateTo(
      routes.PRIVACY_GUIDE,
      /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
  return microtasksFinished();
}

// Set all relevant sync status and fire a changed event and flush the UI.
export function setupSync({
  syncBrowserProxy,
  signedInState,
  syncAllDataTypes,
  typedUrlsSynced,
}: {
  syncBrowserProxy: TestSyncBrowserProxy,
  signedInState: SignedInState,
  syncAllDataTypes: boolean,
  typedUrlsSynced: boolean,
}): void {
  if (syncAllDataTypes) {
    assertTrue(typedUrlsSynced);
  }
  if (typedUrlsSynced) {
    assertTrue(signedInState !== SignedInState.SIGNED_OUT);
  }
  syncBrowserProxy.testSyncStatus = {
    signedInState: signedInState,
    hasError: false,
    statusAction: StatusAction.NO_ACTION,
  };
  webUIListenerCallback('sync-status-changed', syncBrowserProxy.testSyncStatus);

  const event = getSyncAllPrefs();
  // Overwrite datatypes needed in tests.
  event.syncAllDataTypes = syncAllDataTypes;
  event.typedUrlsSynced = typedUrlsSynced;
  webUIListenerCallback('sync-prefs-changed', event);
}

export function setFirstPartyCookieSetting(setting: ContentSetting) {
  PrefService.getInstance().setPrefValue(
      'generated.cookie_default_content_setting', setting);
}

export function setThirdPartyCookieSetting(setting: CookieControlsMode): void {
  PrefService.getInstance().setPrefValue(
      'profile.cookie_controls_mode', setting);
}

export function setThirdPartyCookieBlockingSetting(
    setting: ThirdPartyCookieBlockingSetting): void {
  PrefService.getInstance().setPrefValue(
      'generated.third_party_cookie_blocking_setting', setting);
}

export function shouldShowCookiesCard(): boolean {
  return PrefService.getInstance()
             .getPref<ContentSetting>(
                 'generated.cookie_default_content_setting')
             .value !== ContentSetting.BLOCK;
}

// Set the safe browsing setting for the privacy guide.
export function setSafeBrowsingSetting(setting: SafeBrowsingSetting): void {
  PrefService.getInstance().setPrefValue('generated.safe_browsing', setting);
}

export function shouldShowSafeBrowsingCard(): boolean {
  const setting = PrefService.getInstance()
                      .getPref<SafeBrowsingSetting>('generated.safe_browsing')
                      .value;
  return setting === SafeBrowsingSetting.ENHANCED ||
      setting === SafeBrowsingSetting.STANDARD;
}

export function shouldShowHistorySyncCard(
    syncBrowserProxy: TestSyncBrowserProxy): boolean {
  if (!syncBrowserProxy.testSyncStatus) {
    return true;
  }
  return syncBrowserProxy.testSyncStatus.signedInState ===
      SignedInState.SYNCING ||
      (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') &&
       syncBrowserProxy.testSyncStatus.signedInState ===
           SignedInState.SIGNED_IN);
}

// Bundles functionality to create the page object for tests.
export async function createPrivacyGuidePageForTest(
    syncBrowserProxy: TestSyncBrowserProxy):
    Promise<SettingsPrivacyGuidePageElement> {
  const prefsBrowserProxy =
      new TestPrefsBrowserProxy(getInitialPrivacyGuideTestPrefs());
  PrefsBrowserProxy.setInstance(prefsBrowserProxy);
  PrefService.resetInstanceForTesting();
  await PrefService.getInstance().whenInitialized();

  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const page = document.createElement('settings-privacy-guide-page');
  page.disableAnimationsForTesting();
  document.body.appendChild(page);

  setupPrivacyRouteForTest();
  await microtasksFinished();

  setupSync({
    syncBrowserProxy: syncBrowserProxy,
    signedInState: SignedInState.SYNCING,
    syncAllDataTypes: true,
    typedUrlsSynced: true,
  });

  return page;
}

export function setParametersForHistorySyncStep(
    syncBrowserProxy: TestSyncBrowserProxy, isEligible: boolean): void {
  if (!isEligible) {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SIGNED_OUT,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
  }
  assertEquals(
      isEligible, shouldShowHistorySyncCard(syncBrowserProxy),
      'Parameters for HistorySync are set incorrectly.');
}

export function setParametersForSafeBrowsingStep(isEligible: boolean): void {
  setSafeBrowsingSetting(
      isEligible ? SafeBrowsingSetting.STANDARD : SafeBrowsingSetting.DISABLED);
  assertEquals(
      isEligible, shouldShowSafeBrowsingCard(),
      'Parameters for SafeBrowsing are set incorrectly.');
}

export function setParametersForCookiesStep(isEligible: boolean): void {
  setThirdPartyCookieSetting(CookieControlsMode.BLOCK_THIRD_PARTY);
  if (!isEligible) {
    setFirstPartyCookieSetting(ContentSetting.BLOCK);
  } else {
    setFirstPartyCookieSetting(ContentSetting.ALLOW);
  }
  assertEquals(
      isEligible, shouldShowCookiesCard(),
      'Parameters for Cookies are set incorrectly.');
}

export function clickNextOnWelcomeStep(page: SettingsPrivacyGuidePageElement):
    Promise<void> {
  const welcomeFragment = page.shadowRoot.querySelector<HTMLElement>(
      '#' + PrivacyGuideStep.WELCOME);
  assertTrue(!!welcomeFragment, 'Welcome fragment is null.');
  assertTrue(isChildVisible(page, '#' + PrivacyGuideStep.WELCOME));
  welcomeFragment.dispatchEvent(
      new CustomEvent('start-button-click', {bubbles: true, composed: true}));
  return microtasksFinished();
}
