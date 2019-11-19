// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.ChromeCleanupProxy} */
class TestChromeCleanupProxy extends TestBrowserProxy {
  constructor() {
    super([
      'registerChromeCleanerObserver',
      'restartComputer',
      'startCleanup',
      'startScanning',
      'notifyShowDetails',
      'notifyLearnMoreClicked',
      'getMoreItemsPluralString',
      'getItemsToRemovePluralString',
    ]);
  }

  /** @override */
  registerChromeCleanerObserver() {
    this.methodCalled('registerChromeCleanerObserver');
  }

  /** @override */
  restartComputer() {
    this.methodCalled('restartComputer');
  }

  /** @override */
  startCleanup(logsUploadEnabled) {
    this.methodCalled('startCleanup', logsUploadEnabled);
  }

  /** @override */
  startScanning(logsUploadEnabled) {
    this.methodCalled('startScanning', logsUploadEnabled);
  }

  /** @override */
  notifyShowDetails(enabled) {
    this.methodCalled('notifyShowDetails', enabled);
  }

  /** @override */
  notifyLearnMoreClicked() {
    this.methodCalled('notifyLearnMoreClicked');
  }

  /** @override */
  getMoreItemsPluralString(numHiddenItems) {
    this.methodCalled('getMoreItemsPluralString', numHiddenItems);
    return Promise.resolve('');
  }

  /** @override */
  getItemsToRemovePluralString(numItems) {
    this.methodCalled('getItemsToRemovePluralString', numItems);
    return Promise.resolve('');
  }
}

let chromeCleanupPage = null;

/** @type {?TestDownloadsBrowserProxy} */
let chromeCleanupProxy = null;

const shortFileList = [
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
const shortExtensionList = ['ext 1', 'ext 2'];
const exactSizeExtensionList = ['ext 1', 'ext 2', 'ext 3', 'ext 4'];
const longExtensionList =
    ['ext 1', 'ext 2', 'ext 3', 'ext 4', 'ext 5', 'ext 6'];

const fileLists = [[], shortFileList, exactSizeFileList, longFileList];
const registryKeysLists = [
  [], shortRegistryKeysList, exactSizeRegistryKeysList, longRegistryKeysList
];
const extensionLists =
    [[], shortExtensionList, exactSizeExtensionList, longExtensionList];
const descriptors = ['No', 'Few', 'ExactSize', 'Many'];

const defaultScannerResults = {
  'files': shortFileList,
  'registryKeys': shortRegistryKeysList,
  'extensions': shortExtensionList,
};

/**
 * @param {!Array} originalItems
 * @param {!Array} visibleItems
 */
function validateVisibleItemsList(originalItems, visibleItems) {
  let visibleItemsList =
      visibleItems.shadowRoot.querySelectorAll('.visible-item');
  const moreItemsLink = visibleItems.$$('#more-items-link');

  if (originalItems.length <= settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW) {
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
  } else {
    assertEquals(
        visibleItemsList.length,
        settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);
    assertFalse(moreItemsLink.hidden);

    // Tapping on the "show more" link should expand the list.
    moreItemsLink.click();
    Polymer.dom.flush();

    visibleItemsList =
        visibleItems.shadowRoot.querySelectorAll('.visible-item');
    assertEquals(visibleItemsList.length, originalItems.length);
    assertTrue(moreItemsLink.hidden);
  }
}

/**
 * @param {!Array} originalItems
 * @param {!Element} container
 * @param {boolean} expectSuffix Whether a highlight suffix should exist.
 */
function validateHighlightSuffix(originalItems, container, expectSuffix) {
  const itemList =
      container.shadowRoot.querySelectorAll('li:not(#more-items-link)');
  assertEquals(originalItems.length, itemList.length);
  for (const item of itemList) {
    const suffixes = item.querySelectorAll('.highlight-suffix');
    assertEquals(suffixes.length, 1);
    assertEquals(expectSuffix, !suffixes[0].hidden);
  }
}

/**
 * @param {!Array} files The list of files to be cleaned.
 * @param {!Array} registryKeys The list of registry entries to be cleaned.
 * @param {!Array} extensions The list of extensions to be cleaned.
 */
function startCleanupFromInfected(files, registryKeys, extensions) {
  const scannerResults = {
    'files': files,
    'registryKeys': registryKeys,
    'extensions': extensions
  };

  updateReportingEnabledPref(false);
  cr.webUIListenerCallback(
      'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
      scannerResults);
  Polymer.dom.flush();

  const showItemsButton = chromeCleanupPage.$$('#show-items-button');
  assertTrue(!!showItemsButton);
  showItemsButton.click();

  const filesToRemoveList = chromeCleanupPage.$$('#files-to-remove-list');
  assertTrue(!!filesToRemoveList);
  validateVisibleItemsList(files, filesToRemoveList);
  validateHighlightSuffix(files, filesToRemoveList, true /* expectSuffix */);

  const registryKeysListContainer = chromeCleanupPage.$$('#registry-keys-list');
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

  const extensionsListContainer = chromeCleanupPage.$$('#extensions-list');
  assertTrue(!!extensionsListContainer);
  if (extensions.length > 0) {
    assertFalse(extensionsListContainer.hidden);
    assertTrue(!!extensionsListContainer);
    validateVisibleItemsList(extensions, extensionsListContainer);
    validateHighlightSuffix(
        extensions, extensionsListContainer, false /* expectSuffix */);
  } else {
    assertTrue(extensionsListContainer.hidden);
  }

  const actionButton = chromeCleanupPage.$$('#action-button');
  assertTrue(!!actionButton);
  actionButton.click();
  return chromeCleanupProxy.whenCalled('startCleanup')
      .then(function(logsUploadEnabled) {
        assertFalse(logsUploadEnabled);
        cr.webUIListenerCallback(
            'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
            defaultScannerResults);
        Polymer.dom.flush();

        const spinner = chromeCleanupPage.$$('#waiting-spinner');
        assertTrue(spinner.active);
      });
}

/**
 * @param {boolean} newValue The new value to set to
 *     prefs.software_reporter.reporting.
 */
function updateReportingEnabledPref(newValue) {
  chromeCleanupPage.prefs = {
    software_reporter: {
      reporting: {
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: newValue,
      },
    },
  };
}

/**
 * @param {boolean} testingScanOffered Whether to test the case where scanning
 *     is offered to the user.
 */
function testLogsUploading(testingScanOffered) {
  if (testingScanOffered) {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-infected', true /* isPoweredByPartner */,
        defaultScannerResults);
  } else {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
  }
  Polymer.dom.flush();

  const logsControl = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');

  updateReportingEnabledPref(true);
  assertTrue(!!logsControl);
  assertTrue(logsControl.checked);

  logsControl.$.checkbox.click();
  assertFalse(logsControl.checked);
  assertFalse(chromeCleanupPage.prefs.software_reporter.reporting.value);

  logsControl.$.checkbox.click();
  assertTrue(logsControl.checked);
  assertTrue(chromeCleanupPage.prefs.software_reporter.reporting.value);

  updateReportingEnabledPref(false);
  assertFalse(logsControl.checked);
}

/**
 * @param {boolean} onInfected Whether to test the case where current state is
 *     INFECTED, as opposed to CLEANING.
 * @param {boolean} isPoweredByPartner Whether to test the case when scan
 *     results are provided by a partner.
 */
function testPartnerLogoShown(onInfected, isPoweredByPartner) {
  cr.webUIListenerCallback(
      onInfected ? 'chrome-cleanup-on-infected' : 'chrome-cleanup-on-cleaning',
      isPoweredByPartner, defaultScannerResults);
  Polymer.dom.flush();

  const poweredByContainerControl =
      chromeCleanupPage.$$('#powered-by-container');
  assertTrue(!!poweredByContainerControl);
  assertNotEquals(poweredByContainerControl.hidden, isPoweredByPartner);
}

suite('ChromeCleanupHandler', function() {
  setup(function() {
    chromeCleanupProxy = new TestChromeCleanupProxy();
    settings.ChromeCleanupProxyImpl.instance_ = chromeCleanupProxy;

    PolymerTest.clearBody();

    chromeCleanupPage = document.createElement('settings-chrome-cleanup-page');
    chromeCleanupPage.prefs = {
      software_reporter: {
        reporting: {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
    };
    document.body.appendChild(chromeCleanupPage);
  });

  function scanOfferedOnInitiallyIdle(idleReason) {
    cr.webUIListenerCallback('chrome-cleanup-on-idle', idleReason);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
  }

  test('scanOfferedOnInitiallyIdle_ReporterFoundNothing', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ReporterFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.REPORTER_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFoundNothing', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
  });

  test('scanOfferedOnInitiallyIdle_ScanningFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.SCANNING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_ConnectionLost', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CONNECTION_LOST);
  });

  test('scanOfferedOnInitiallyIdle_UserDeclinedCleanup', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.USER_DECLINED_CLEANUP);
  });

  test('scanOfferedOnInitiallyIdle_CleaningFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANING_FAILED);
  });

  test('scanOfferedOnInitiallyIdle_CleaningSucceeded', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
  });

  test('scanOfferedOnInitiallyIdle_CleanerDownloadFailed', function() {
    scanOfferedOnInitiallyIdle(
        settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
  });

  test('cleanerDownloadFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    return chromeCleanupProxy.whenCalled('startScanning');
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('reporterFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reporter-running');
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('startScanFromIdle', function() {
    updateReportingEnabledPref(false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    return chromeCleanupProxy.whenCalled('startScanning')
        .then(function(logsUploadEnabled) {
          assertFalse(logsUploadEnabled);
          cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
          Polymer.dom.flush();

          const spinner = chromeCleanupPage.$$('#waiting-spinner');
          assertTrue(spinner.active);
        });
  });

  test('scanFoundNothing', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('scanFailure', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-scanning', false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.SCANNING_FAILED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  // Test all combinations of item list sizes.
  for (let file_index = 0; file_index < fileLists.length; file_index++) {
    for (let registry_index = 0; registry_index < registryKeysLists.length;
         registry_index++) {
      for (let extension_index = 0; extension_index < extensionLists.length;
           extension_index++) {
        const testName = 'startCleanupFromInfected_' + descriptors[file_index] +
            'Files' + descriptors[registry_index] + 'RegistryKeys' +
            descriptors[extension_index] + 'Extensions';
        const fileList = fileLists[file_index];
        const registryKeysList = registryKeysLists[registry_index];
        const extensionList = extensionLists[extension_index];

        test(testName, function() {
          return startCleanupFromInfected(
              fileList, registryKeysList, extensionList);
        });
      }
    }
  }

  test('rebootFromRebootRequired', function() {
    cr.webUIListenerCallback('chrome-cleanup-on-reboot-required');
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    return chromeCleanupProxy.whenCalled('restartComputer');
  });

  test('cleanupFailure', function() {
    updateReportingEnabledPref(false);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
        defaultScannerResults);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANING_FAILED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);
  });

  test('cleanupSuccess', function() {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-cleaning', true /* isPoweredByPartner */,
        defaultScannerResults);
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle',
        settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertFalse(!!actionButton);

    const title = chromeCleanupPage.$$('#status-title');
    assertTrue(!!title);
    assertTrue(!!title.querySelector('a'));
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
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
    // prefs.software_reporter.enabled is not a real preference as it can't be
    // set by the user. ChromeCleanupHandler can notify the JS of changes to the
    // policy enforcement.
    cr.webUIListenerCallback('chrome-cleanup-enabled-change', false);
    Polymer.dom.flush();

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    const logsControl = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');
    assertTrue(!!logsControl);
    assertTrue(logsControl.disabled);
  });

  test('logsUploadingState_reporterReportingPolicyDisabled', function() {
    cr.webUIListenerCallback(
        'chrome-cleanup-on-idle', settings.ChromeCleanupIdleReason.INITIAL);
    Polymer.dom.flush();

    chromeCleanupPage.prefs = {
      software_reporter: {
        reporting: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
          value: false,
        }
      },
    };

    const actionButton = chromeCleanupPage.$$('#action-button');
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    const logsControl = chromeCleanupPage.$$('#chromeCleanupLogsUploadControl');
    assertTrue(!!logsControl);
    assertFalse(logsControl.disabled);
    assertTrue(logsControl.$.checkbox.disabled);
  });
});
