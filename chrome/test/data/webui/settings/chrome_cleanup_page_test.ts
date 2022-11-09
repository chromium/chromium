// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW, ChromeCleanerScannerResults, ChromeCleanupFilePath, ChromeCleanupIdleReason, ChromeCleanupProxyImpl, ItemsToRemoveListElement, SettingsCheckboxElement, SettingsChromeCleanupPageElement} from 'chrome://settings/lazy_load.js';
import {CrButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestChromeCleanupProxy} from './test_chrome_cleanup_proxy.js';

// clang-format on

let chromeCleanupPage: SettingsChromeCleanupPageElement;
let chromeCleanupProxy: TestChromeCleanupProxy;

const shortFileList: ChromeCleanupFilePath[] = [
  {'dirname': 'C:\\', 'basename': 'file 1'},
  {'dirname': 'C:\\', 'basename': 'file 2'},
  {'dirname': 'C:\\', 'basename': 'file 3'},
];
const exactSizeFileList =
    shortFileList.concat([{'dirname': 'C:\\', 'basename': 'file 4'}]);
const longFileList =
    exactSizeFileList.concat([{'dirname': 'C:\\', 'basename': 'file 5'}]);
const shortRegistryKeysList = ['key 1', 'key 2'];
const exactSizeRegistryKeysList = ['key 1', 'key 2', 'key 3', 'key 4'];
const longRegistryKeysList =
    ['key 1', 'key 2', 'key 3', 'key 4', 'key 5', 'key 6'];

const fileLists = [[], shortFileList, exactSizeFileList, longFileList];
const registryKeysLists = [
  [],
  shortRegistryKeysList,
  exactSizeRegistryKeysList,
  longRegistryKeysList,
];
const descriptors = ['No', 'Few', 'ExactSize', 'Many'];

const defaultScannerResults: ChromeCleanerScannerResults = {
  files: shortFileList,
  registryKeys: shortRegistryKeysList,
};

function validateVisibleItemsList(
    originalItems: Array<string|ChromeCleanupFilePath>,
    visibleItems: ItemsToRemoveListElement) {
  let visibleItemsList =
      visibleItems.shadowRoot!.querySelectorAll('.visible-item');
  const moreItemsLink =
      visibleItems.shadowRoot!.querySelector<HTMLElement>('#more-items-link')!;

  if (originalItems.length <= CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW) {
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
  } else {
    assertEquals(
        visibleItemsList.length, CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);
    assertFalse(moreItemsLink.hidden);

    // Tapping on the "show more" link should expand the list.
    moreItemsLink.click();
    flush();

    visibleItemsList =
        visibleItems.shadowRoot!.querySelectorAll('.visible-item');
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
  }
}

/**
 * @param expectSuffix Whether a highlight suffix should exist.
 */
function validateHighlightSuffix(
    originalItems: Array<string|ChromeCleanupFilePath>,
    container: ItemsToRemoveListElement, expectSuffix: boolean) {
  const itemList =
      container.shadowRoot!.querySelectorAll('li:not(#more-items-link)');
  assertEquals(originalItems.length, itemList.length);
  for (const item of itemList) {
    const suffixes = item.querySelectorAll<HTMLElement>('.highlight-suffix');
    assertEquals(suffixes.length, 1);
    assertEquals(expectSuffix, !suffixes[0]!.hidden);
  }
}

/**
 * @param files The list of files to be cleaned.
 * @param registryKeys The list of registry entries to be cleaned.
 */
async function startCleanupFromInfected(
    files: ChromeCleanupFilePath[], registryKeys: string[]) {
  const scannerResults: ChromeCleanerScannerResults = {files, registryKeys};

  updateReportingEnabledPref(false);
  webUIListenerCallback(
      'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
      scannerResults);
  flush();

  const showItemsButton =
      chromeCleanupPage.shadowRoot!.querySelector<HTMLElement>(
          '#show-items-button')!;
  assertTrue(!!showItemsButton);
  showItemsButton.click();

  const filesToRemoveList =
      chromeCleanupPage.shadowRoot!.querySelector<ItemsToRemoveListElement>(
          '#files-to-remove-list')!;
  assertTrue(!!filesToRemoveList);
  validateVisibleItemsList(files, filesToRemoveList);
  validateHighlightSuffix(files, filesToRemoveList, true /* expectSuffix */);

  const registryKeysListContainer =
      chromeCleanupPage.shadowRoot!.querySelector<ItemsToRemoveListElement>(
          '#registry-keys-list')!;
  assertTrue(!!registryKeysListContainer);
  if (registryKeys.length > 0) {
    assertFalse(registryKeysListContainer.hidden);
    assertTrue(!!registryKeysListContainer);
    validateVisibleItemsList(registryKeys, registryKeysListContainer);
    validateHighlightSuffix(
        registryKeys, registryKeysListContainer, false /* expectSuffix */);
  } else {
    assertTrue(registryKeysListContainer.hidden);
  }

  const actionButton =
      chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
          '#action-button');
  assertTrue(!!actionButton);
  actionButton!.click();
  const logsUploadEnabled = await chromeCleanupProxy.whenCalled('startCleanup');
  assertFalse(logsUploadEnabled);
  webUIListenerCallback(
      'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
      defaultScannerResults);
  flush();

  const spinner =
      chromeCleanupPage.shadowRoot!.querySelector('paper-spinner-lite')!;
  assertTrue(spinner.active);
}

/**
 * @param newValue The new value to set to
 *     prefs.software_reporter.reporting.
 */
function updateReportingEnabledPref(newValue: boolean) {
  chromeCleanupPage.prefs = {
    software_reporter: {
      reporting: {
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: newValue,
        key: '',
      },
    },
  };
}

/**
 * @param testingScanOffered Whether to test the case where scanning
 *     is offered to the user.
 */
function testLogsUploading(testingScanOffered: boolean) {
  if (testingScanOffered) {
    webUIListenerCallback(
        'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
        defaultScannerResults);
  } else {
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.INITIAL);
  }
  flush();

  const logsControl =
      chromeCleanupPage.shadowRoot!.querySelector<SettingsCheckboxElement>(
          '#chromeCleanupLogsUploadControl');

  updateReportingEnabledPref(true);
  assertTrue(!!logsControl);
  assertTrue(logsControl!.checked);

  logsControl!.$.checkbox.click();
  assertFalse(logsControl!.checked);
  assertFalse(chromeCleanupPage.prefs.software_reporter.reporting.value);

  logsControl!.$.checkbox.click();
  assertTrue(logsControl!.checked);
  assertTrue(chromeCleanupPage.prefs.software_reporter.reporting.value);

  updateReportingEnabledPref(false);
  assertFalse(logsControl!.checked);
}

/**
 * @param onInfected Whether to test the case where current state is
 *     INFECTED, as opposed to CLEANING.
 * @param isPoweredByPartner Whether to test the case when scan
 *     results are provided by a partner.
 */
function testPartnerLogoShown(
    onInfected: boolean, isPoweredByPartner: boolean) {
  webUIListenerCallback(
      onInfected ? 'chrome-cleanup-on-infected' : 'chrome-cleanup-on-cleaning',
      isPoweredByPartner, defaultScannerResults);
  flush();

  const poweredByContainerControl =
      chromeCleanupPage.shadowRoot!.querySelector<HTMLElement>('#powered-by');
  assertTrue(!!poweredByContainerControl);
  assertNotEquals(poweredByContainerControl!.hidden, isPoweredByPartner);
}

suite('ChromeCleanupHandler', function() {
  setup(function() {
    chromeCleanupProxy = new TestChromeCleanupProxy();
    ChromeCleanupProxyImpl.setInstance(chromeCleanupProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    chromeCleanupPage = document.createElement('settings-chrome-cleanup-page');
    chromeCleanupPage.prefs = {
      software_reporter: {
        reporting: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
          key: '',
        },
      },
    };
    document.body.appendChild(chromeCleanupPage);
  });

  function scanOfferedOnInitiallyIdle(idleReason: ChromeCleanupIdleReason) {
    webUIListenerCallback('chrome-cleanup-on-idle', idleReason);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertTrue(!!actionButton);
  }

  test('scanOfferedOnInitiallyIdle_ReporterFoundNothing', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ReporterFailed', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.REPORTER_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFoundNothing', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFailed', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.SCANNING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ConnectionLost', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.CONNECTION_LOST);
  });

  test('scanOfferedOnInitiallyIdle_UserDeclinedCleanup', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.USER_DECLINED_CLEANUP);
  });

  test('scanOfferedOnInitiallyIdle_CleaningFailed', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.CLEANING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_CleaningSucceeded', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
  });

  test('scanOfferedOnInitiallyIdle_CleanerDownloadFailed', function() {
    scanOfferedOnInitiallyIdle(ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
  });

  test('cleanerDownloadFailure', function() {
    webUIListenerCallback('chrome-cleanup-on-reporter-running');
    webUIListenerCallback(
        'chrome-cleanup-on-idle',
        ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
            '#action-button');
    assertTrue(!!actionButton);
    actionButton!.click();
    return chromeCleanupProxy.whenCalled('startScanning');
  });

  test('reporterFoundNothing', function() {
    webUIListenerCallback('chrome-cleanup-on-reporter-running');
    webUIListenerCallback(
        'chrome-cleanup-on-idle',
        ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);
  });

  test('reporterFoundNothing', function() {
    webUIListenerCallback('chrome-cleanup-on-reporter-running');
    webUIListenerCallback(
        'chrome-cleanup-on-idle',
        ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);
  });

  test('startScanFromIdle', async function() {
    updateReportingEnabledPref(false);
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.INITIAL);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
            '#action-button');
    assertTrue(!!actionButton);
    actionButton!.click();
    const logsUploadEnabled =
        await chromeCleanupProxy.whenCalled('startScanning');
    assertFalse(logsUploadEnabled);
    webUIListenerCallback('chrome-cleanup-on-scanning', false);
    flush();

    const spinner =
        chromeCleanupPage.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertTrue(spinner.active);
  });

  test('scanFoundNothing', function() {
    webUIListenerCallback('chrome-cleanup-on-scanning', false);
    webUIListenerCallback(
        'chrome-cleanup-on-idle',
        ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);
  });

  test('scanFailure', function() {
    webUIListenerCallback('chrome-cleanup-on-scanning', false);
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.SCANNING_FAILED);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);
  });

  // Test all combinations of item list sizes.
  for (let file_index = 0; file_index < fileLists.length; file_index++) {
    for (let registry_index = 0; registry_index < registryKeysLists.length;
         registry_index++) {
      const testName = 'startCleanupFromInfected_' + descriptors[file_index] +
          'Files' + descriptors[registry_index] + 'RegistryKeys';
      const fileList = fileLists[file_index]!;
      const registryKeysList = registryKeysLists[registry_index]!;

      test(testName, async function() {
        await startCleanupFromInfected(fileList, registryKeysList);
      });
    }
  }

  test('rebootFromRebootRequired', function() {
    webUIListenerCallback('chrome-cleanup-on-reboot-required');
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
            '#action-button');
    assertTrue(!!actionButton);
    actionButton!.click();
    return chromeCleanupProxy.whenCalled('restartComputer');
  });

  test('cleanupFailure', function() {
    updateReportingEnabledPref(false);
    webUIListenerCallback(
        'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
        defaultScannerResults);
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.CLEANING_FAILED);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);
  });

  test('cleanupSuccess', function() {
    webUIListenerCallback(
        'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
        defaultScannerResults);
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector('#action-button');
    assertFalse(!!actionButton);

    const title = chromeCleanupPage.shadowRoot!.querySelector('#status-title');
    assertTrue(!!title);
    assertTrue(!!title!.querySelector('a'));
  });

  test('logsUploadingOnScanOffered', function() {
    return testLogsUploading(true /* testingScanOffered */);
  });

  test('logsUploadingOnInfected', function() {
    return testLogsUploading(false /* testingScanOffered */);
  });

  test('onInfectedResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        true /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onInfectedResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        true /* onInfected */, false /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_True', function() {
    return testPartnerLogoShown(
        false /* onInfected */, true /* isPoweredByPartner */);
  });

  test('onCleaningResultsProvidedByPartner_False', function() {
    return testPartnerLogoShown(
        false /* onInfected */, false /* isPoweredByPartner */);
  });

  test('logsUploadingState_reporterPolicyDisabled', function() {
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.INITIAL);
    // prefs.software_reporter.enabled is not a real preference as it can't be
    // set by the user. ChromeCleanupHandler can notify the JS of changes to the
    // policy enforcement.
    webUIListenerCallback('chrome-cleanup-enabled-change', false);
    flush();

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
            '#action-button');
    assertTrue(!!actionButton);
    assertTrue(actionButton!.disabled);

    const logsControl =
        chromeCleanupPage.shadowRoot!.querySelector<SettingsCheckboxElement>(
            '#chromeCleanupLogsUploadControl');
    assertTrue(!!logsControl);
    assertTrue(logsControl!.disabled);
  });

  test('logsUploadingState_reporterReportingPolicyDisabled', function() {
    webUIListenerCallback(
        'chrome-cleanup-on-idle', ChromeCleanupIdleReason.INITIAL);
    flush();

    chromeCleanupPage.prefs = {
      software_reporter: {
        reporting: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
          value: false,
          key: '',
        },
      },
    };

    const actionButton =
        chromeCleanupPage.shadowRoot!.querySelector<CrButtonElement>(
            '#action-button');
    assertTrue(!!actionButton);
    assertFalse(actionButton!.disabled);

    const logsControl =
        chromeCleanupPage.shadowRoot!.querySelector<SettingsCheckboxElement>(
            '#chromeCleanupLogsUploadControl');
    assertTrue(!!logsControl);
    assertFalse(logsControl!.disabled);
    assertTrue(logsControl!.$.checkbox.disabled);
  });
});
