// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ClearBrowsingDataResult, SettingsCheckboxElement, SettingsClearBrowsingDataDialogElement, SettingsHistoryDeletionDialogElement, SettingsPasswordsDeletionDialogElement} from 'chrome://settings/lazy_load.js';
import {ClearBrowsingDataBrowserProxyImpl, TimePeriodExperiment, TimePeriod} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement, SettingsDropdownMenuElement} from 'chrome://settings/settings.js';
import {loadTimeData, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible, eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// <if expr="not is_chromeos">
import {Router, routes} from 'chrome://settings/settings.js';
// </if>

// clang-format on

function getClearBrowsingDataPrefs() {
  return {
    browser: {
      clear_data: {
        browsing_history: {
          key: 'browser.clear_data.browsing_history',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        browsing_history_basic: {
          key: 'browser.clear_data.browsing_history_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cache: {
          key: 'browser.clear_data.cache',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cache_basic: {
          key: 'browser.clear_data.cache_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cookies: {
          key: 'browser.clear_data.cookies',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cookies_basic: {
          key: 'browser.clear_data.cookies_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        download_history: {
          key: 'browser.clear_data.download_history',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        hosted_apps_data: {
          key: 'browser.clear_data.hosted_apps_data',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        form_data: {
          key: 'browser.clear_data.form_data',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        passwords: {
          key: 'browser.clear_data.passwords',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        site_settings: {
          key: 'browser.clear_data.site_settings',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        time_period: {
          key: 'browser.clear_data.time_period',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        time_period_basic: {
          key: 'browser.clear_data.time_period_basic',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        time_period_v2: {
          key: 'browser.clear_data.time_period_v2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: TimePeriodExperiment.NOT_SELECTED,
        },
        time_period_v2_basic: {
          key: 'browser.clear_data.time_period_v2_basic',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: TimePeriodExperiment.NOT_SELECTED,
        },
      },
      last_clear_browsing_data_tab: {
        key: 'browser.last_clear_browsing_data_tab',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 0,
      },
    },
  };
}

suite('ClearBrowsingDataDesktop', function() {
  let testBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let testSyncBrowserProxy: TestSyncBrowserProxy;
  let element: SettingsClearBrowsingDataDialogElement;

  setup(async function() {
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // TODO(b/314968275): Add tests for when UNO Desktop is enabled.
    loadTimeData.overrideValues(
        {enableCbdTimeframeRequired: false, unoDesktopEnabled: false});
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    await testBrowserProxy.whenCalled('initialize');
    assertTrue(element.$.clearBrowsingDataDialog.open);
  });

  teardown(function() {
    element.remove();
  });

  test('ClearBrowsingDataSyncAccountInfoDesktop', function() {
    // Not syncing: the footer is hidden.
    webUIListenerCallback('sync-status-changed', {
      signedIn: false,
      hasError: false,
    });
    flush();
    assertFalse(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));

    // The footer is never shown on Lacros.
    // <if expr="not is_chromeos">
    // Syncing: the footer is shown, with the normal sync info.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: false,
    });
    flush();
    assertTrue(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    assertTrue(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Sync is paused.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.REAUTHENTICATE,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertTrue(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Sync passphrase error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertTrue(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Other sync error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertTrue(isChildVisible(element, '#sync-other-error-info'));
    // </if>
  });

  // The footer is never shown on Lacros.
  // <if expr="not is_chromeos">
  test('ClearBrowsingDataPauseSyncDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: false,
    });
    flush();
    assertTrue(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo = element.shadowRoot!.querySelector('#sync-info');
    assertTrue(!!syncInfo);
    assertTrue(isVisible(syncInfo));
    const signoutLink = syncInfo.querySelector<HTMLElement>('a[href]');
    assertTrue(!!signoutLink);
    assertEquals(0, testSyncBrowserProxy.getCallCount('pauseSync'));
    signoutLink.click();
    assertEquals(1, testSyncBrowserProxy.getCallCount('pauseSync'));
  });

  test('ClearBrowsingDataStartSignInDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.REAUTHENTICATE,
    });
    flush();
    assertTrue(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo = element.shadowRoot!.querySelector('#sync-paused-info');
    assertTrue(!!syncInfo);
    assertTrue(isVisible(syncInfo));
    const signinLink = syncInfo.querySelector<HTMLElement>('a[href]');
    assertTrue(!!signinLink);
    assertEquals(0, testSyncBrowserProxy.getCallCount('startSignIn'));
    signinLink.click();
    assertEquals(1, testSyncBrowserProxy.getCallCount('startSignIn'));
  });

  test('ClearBrowsingDataHandlePassphraseErrorDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    flush();
    assertTrue(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo =
        element.shadowRoot!.querySelector('#sync-passphrase-error-info');
    assertTrue(!!syncInfo);
    assertTrue(isVisible(syncInfo));
    const passphraseLink = syncInfo.querySelector<HTMLElement>('a[href]');
    assertTrue(!!passphraseLink);
    passphraseLink.click();
    assertEquals(routes.SYNC, Router.getInstance().getCurrentRoute());
  });
  // </if>

  test('ClearBrowsingDataSearchLabelVisibility', function() {
    for (const signedIn of [false, true]) {
      for (const isNonGoogleDse of [false, true]) {
        webUIListenerCallback('update-sync-state', {
          signedIn: signedIn,
          isNonGoogleDse: isNonGoogleDse,
          nonGoogleSearchHistoryString: 'Some test string',
        });
        flush();
        // Test Google search history label visibility and string.
        assertEquals(
            signedIn,
            isVisible(
                element.shadowRoot!.querySelector('#googleSearchHistoryLabel')),
            'googleSearchHistoryLabel visibility');
        if (signedIn) {
          assertEquals(
              isNonGoogleDse ?
                  element.i18nAdvanced('clearGoogleSearchHistoryNonGoogleDse')
                      .toString() :
                  element.i18nAdvanced('clearGoogleSearchHistoryGoogleDse')
                      .toString(),
              element.shadowRoot!
                  .querySelector<HTMLElement>(
                      '#googleSearchHistoryLabel')!.innerHTML,
              'googleSearchHistoryLabel text');
        }
        // Test non-Google search history label visibility and string.
        assertEquals(
            isNonGoogleDse,
            isVisible(element.shadowRoot!.querySelector(
                '#nonGoogleSearchHistoryLabel')),
            'nonGoogleSearchHistoryLabel visibility');
        if (isNonGoogleDse) {
          assertEquals(
              'Some test string',
              element.shadowRoot!
                  .querySelector<HTMLElement>(
                      '#nonGoogleSearchHistoryLabel')!.innerText,
              'nonGoogleSearchHistoryLabel text');
        }
      }
    }
  });

  test('ClearBrowsingData_MenuOptions', async function() {
    // The user selects the tab of interest.
    element.$.tabs.selected = 0;
    await element.$.tabs.updateComplete;

    const page = element.$.pages.selectedItem as HTMLElement;
    const dropdownMenu =
        page.querySelector<SettingsDropdownMenuElement>('.time-range-select');
    assertTrue(!!dropdownMenu);
    assertTrue(!!dropdownMenu.menuOptions);
    assertTrue(dropdownMenu.menuOptions.length === 5);

    // TODO(crbug.com/1487530): Remove once CbdTimeframeRequired finished.
    assertFalse(dropdownMenu.menuOptions.some(
        option =>
            option.name === loadTimeData.getString('clearPeriod15Minutes')));
  });

  async function testCbdExperimentDropdown(
      testBrowserProxy: TestClearBrowsingDataBrowserProxy, tabIndex: number) {
    // This test requires recreation of the page (ClearBrowsingDataDialog) after
    // defining loadTimeData to apply experiment changes after enabling the
    // feature/flag.
    testBrowserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({enableCbdTimeframeRequired: true});
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    await testBrowserProxy.whenCalled('initialize');
    assertEquals(1, testBrowserProxy.getCallCount('initialize'));
    await flushTasks();

    // The user selects the tab of interest.
    element.$.tabs.selected = tabIndex;
    await element.$.tabs.updateComplete;

    // The dropdown menu contains the "not selected" and "15 min" options on
    // top.
    const page = element.$.pages.selectedItem as HTMLElement;
    const dropdownMenu =
        page.querySelector<SettingsDropdownMenuElement>('.time-range-select');
    assertTrue(!!dropdownMenu);
    assertTrue(!!dropdownMenu.menuOptions);
    assertEquals(7, dropdownMenu.menuOptions.length);
    assertEquals(
        loadTimeData.getString('clearPeriodNotSelected'),
        dropdownMenu.menuOptions[0]!.name);
    assertEquals(
        loadTimeData.getString('clearPeriod15Minutes'),
        dropdownMenu.menuOptions[1]!.name);
    assertEquals(
        loadTimeData.getString('clearPeriodNotSelected'),
        dropdownMenu.$.dropdownMenu
            .options[dropdownMenu.$.dropdownMenu.selectedIndex]!.text);
    for (const option of dropdownMenu.$.dropdownMenu.options) {
      assertEquals(
          option.text === loadTimeData.getString('clearPeriodNotSelected'),
          option.hidden);
    }

    // Select a datatype for deletion to enable the clear button.
    const cookiesCheckbox =
        page.querySelector<SettingsCheckboxElement>('.cookies-checkbox');
    assertTrue(!!cookiesCheckbox);
    cookiesCheckbox.$.checkbox.click();
    await cookiesCheckbox.$.checkbox.updateComplete;
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    // Before trying data clearing without a time range selection, the dropdown
    // is not in the dropdown-error state.
    assertFalse(dropdownMenu.classList.contains('dropdown-error'));

    // Once the user tries to clear data without having made a time range
    // selection the dropdown goes into the dropdown-error state.
    actionButton.click();
    assertTrue(dropdownMenu.classList.contains('dropdown-error'));

    // Once a time range is selected, the dropdown is no longer in the
    // dropdown-error state.
    dropdownMenu.$.dropdownMenu.value = TimePeriod.LAST_DAY.toString();
    dropdownMenu.$.dropdownMenu.dispatchEvent(new CustomEvent('change'));
    await waitAfterNextRender(dropdownMenu);
    assertFalse(dropdownMenu.classList.contains('dropdown-error'));
  }

  // TODO(crbug.com/1487530): Remove once CbdTimeframeRequired finished.
  test('ClearBrowsingData_CbdTimeframeRequired_Basic', function() {
    return testCbdExperimentDropdown(testBrowserProxy, /*tabIndex*/ 0);
  });

  // TODO(crbug.com/1487530): Remove once CbdTimeframeRequired finished.
  test('ClearBrowsingData_CbdTimeframeRequired_Advanced', function() {
    return testCbdExperimentDropdown(testBrowserProxy, /*tabIndex*/ 1);
  });

  async function testUnsupportedTimePeriod(tabIndex: number, prefName: string) {
    // The user selects the tab of interest.
    element.$.tabs.selected = tabIndex;
    await element.$.tabs.updateComplete;

    const page = element.$.pages.selectedItem as HTMLElement;
    const dropdownMenu =
        page.querySelector<SettingsDropdownMenuElement>('.time-range-select');
    assertTrue(!!dropdownMenu);

    const unsupported_pref_value = 100;

    element.setPrefValue(prefName, unsupported_pref_value);

    await waitAfterNextRender(dropdownMenu);

    // Assert unsupported value in Advanced tab is replaced by the Default value
    // (Last hour).
    assertEquals(TimePeriod.LAST_HOUR, element.getPref(prefName).value);
    assertEquals(
        TimePeriod.LAST_HOUR.toString(), dropdownMenu.getSelectedValue());
  }

  test('ClearBrowsingData_UnsupportedTimePeriod_Basic', function() {
    return testUnsupportedTimePeriod(0, 'browser.clear_data.time_period_basic');
  });

  test('ClearBrowsingData_UnsupportedTimePeriod_Advanced', function() {
    return testUnsupportedTimePeriod(1, 'browser.clear_data.time_period');
  });
});

suite('ClearBrowsingDataAllPlatforms', function() {
  let testBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let element: SettingsClearBrowsingDataDialogElement;

  setup(function() {
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    return testBrowserProxy.whenCalled('initialize');
  });

  teardown(function() {
    element.remove();
  });

  async function assertDropdownSelectionPersisted(
      tabIndex: number, prefName: string) {
    assertTrue(element.$.clearBrowsingDataDialog.open);
    // The user selects the tab of interest.
    element.$.tabs.selected = tabIndex;
    await element.$.tabs.updateComplete;

    const page = element.$.pages.selectedItem as HTMLElement;
    const dropdownMenu =
        page.querySelector<SettingsDropdownMenuElement>('.time-range-select');
    assertTrue(!!dropdownMenu);

    // Ensure the test starts with a known pref and dropdown value.
    element.setPrefValue(prefName, TimePeriod.LAST_DAY);
    await waitAfterNextRender(dropdownMenu);
    assertEquals(
        TimePeriod.LAST_DAY.toString(), dropdownMenu.getSelectedValue());

    // Changing the dropdown selection does persist its value to the pref.
    dropdownMenu.$.dropdownMenu.value = TimePeriod.LAST_WEEK.toString();
    dropdownMenu.$.dropdownMenu.dispatchEvent(new CustomEvent('change'));
    await waitAfterNextRender(dropdownMenu);
    assertEquals(TimePeriod.LAST_WEEK, element.getPref(prefName).value);

    // Select a datatype for deletion to enable the clear button.
    assertTrue(!!element.$.cookiesCheckbox);
    element.$.cookiesCheckbox.$.checkbox.click();
    await element.$.cookiesCheckbox.$.checkbox.updateComplete;
    assertTrue(!!element.$.cookiesCheckboxBasic);
    element.$.cookiesCheckboxBasic.$.checkbox.click();
    await element.$.cookiesCheckboxBasic.$.checkbox.updateComplete;
    // Confirming the deletion sends the time range for clearing.
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    const args = await testBrowserProxy.whenCalled('clearBrowsingData');
    const timeRange = args[1];
    assertEquals(TimePeriod.LAST_WEEK, timeRange);
  }

  test('dropdownSelectionPersisted_Basic', function() {
    return assertDropdownSelectionPersisted(
        /*tabIndex*/ 0, 'browser.clear_data.time_period_basic');
  });

  test('dropdownSelectionPersisted_Advanced', function() {
    return assertDropdownSelectionPersisted(
        /*tabIndex*/ 1, 'browser.clear_data.time_period');
  });

  test('tabSelection', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);

    // Ensure the test starts with a known pref state and tab selection.
    element.setPrefValue('browser.last_clear_browsing_data_tab', 0);
    await waitAfterNextRender(element);
    assertEquals(
        0, element.getPref('browser.last_clear_browsing_data_tab').value);
    assertTrue(isChildVisible(element, '#basic-tab'));

    // Changing the tab selection changes the visible tab, but does not persist
    // the tab selection to the pref.
    element.$.tabs.selected = 1;
    await element.$.tabs.updateComplete;
    assertEquals(
        0, element.getPref('browser.last_clear_browsing_data_tab').value);
    assertTrue(isChildVisible(element, '#advanced-tab'));

    // Select a datatype for deletion to enable the clear button.
    assertTrue(!!element.$.cookiesCheckbox);
    element.$.cookiesCheckbox.$.checkbox.click();
    await element.$.cookiesCheckbox.$.checkbox.updateComplete;
    // Confirming the deletion persists the tab selection to the pref.
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    assertEquals(
        1, element.getPref('browser.last_clear_browsing_data_tab').value);
  });

  test('ClearBrowsingDataTap', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);

    const cancelButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.cancel-button');
    assertTrue(!!cancelButton);
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    const spinner = element.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);

    // Select a datatype for deletion to enable the clear button.
    assertTrue(!!element.$.cookiesCheckboxBasic);
    element.$.cookiesCheckboxBasic.$.checkbox.click();
    await element.$.cookiesCheckboxBasic.$.checkbox.updateComplete;

    assertFalse(cancelButton.disabled);
    assertFalse(actionButton.disabled);
    assertFalse(spinner.active);

    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    const args = await testBrowserProxy.whenCalled('clearBrowsingData');
    const dataTypes = args[0];
    assertEquals(1, dataTypes.length);
    assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
    assertTrue(element.$.clearBrowsingDataDialog.open);
    assertTrue(cancelButton.disabled);
    assertTrue(actionButton.disabled);
    assertTrue(spinner.active);

    // Simulate signal from browser indicating that clearing has
    // completed.
    webUIListenerCallback('browsing-data-removing', false);
    // Yields to the message loop to allow the callback chain of the
    // Promise that was just resolved to execute before the
    // assertions.
    promiseResolver.resolve(
        {showHistoryNotice: false, showPasswordsNotice: false});
    await promiseResolver.promise;

    assertFalse(element.$.clearBrowsingDataDialog.open);
    assertFalse(cancelButton.disabled);
    assertFalse(actionButton.disabled);
    assertFalse(spinner.active);
    assertFalse(!!element.shadowRoot!.querySelector('#historyNotice'));
    assertFalse(!!element.shadowRoot!.querySelector('#passwordsNotice'));
  });

  test('ClearBrowsingDataClearButton', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);

    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    assertTrue(!!element.$.cookiesCheckboxBasic);
    // Initially the button is disabled because all checkboxes are off.
    assertTrue(actionButton.disabled);
    // The button gets enabled if any checkbox is selected.
    element.$.cookiesCheckboxBasic.$.checkbox.click();
    await element.$.cookiesCheckboxBasic.$.checkbox.updateComplete;
    assertTrue(element.$.cookiesCheckboxBasic.checked);
    assertFalse(actionButton.disabled);
    // Switching to advanced disables the button.
    element.$.tabs.selected = 1;
    await element.$.tabs.updateComplete;
    assertTrue(actionButton.disabled);
    // Switching back enables it again.
    element.$.tabs.selected = 0;
    await element.$.tabs.updateComplete;
    assertFalse(actionButton.disabled);
  });

  test('showHistoryDeletionDialog', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    assertTrue(!!element.$.cookiesCheckboxBasic);
    element.$.cookiesCheckboxBasic.$.checkbox.click();
    await element.$.cookiesCheckboxBasic.$.checkbox.updateComplete;
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    await testBrowserProxy.whenCalled('clearBrowsingData');
    // Passing showHistoryNotice = true should trigger the notice about
    // other forms of browsing history to open, and the dialog to stay
    // open.


    // Yields to the message loop to allow the callback chain of the
    // Promise that was just resolved to execute before the
    // assertions.
    promiseResolver.resolve(
        {showHistoryNotice: true, showPasswordsNotice: false});
    await promiseResolver.promise;

    flush();
    const notice1 =
        element.shadowRoot!.querySelector<SettingsHistoryDeletionDialogElement>(
            '#historyNotice');
    assertTrue(!!notice1);
    const noticeActionButton =
        notice1.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!noticeActionButton);

    // The notice should have replaced the main dialog.
    assertFalse(element.$.clearBrowsingDataDialog.open);
    assertTrue(notice1.$.dialog.open);

    const whenNoticeClosed = eventToPromise('close', notice1!);

    // Tapping the action button will close the notice.
    noticeActionButton.click();

    await whenNoticeClosed;
    const notice2 = element.shadowRoot!.querySelector('#historyNotice');
    assertFalse(!!notice2);
    assertFalse(element.$.clearBrowsingDataDialog.open);
  });

  test('showPasswordsDeletionDialog', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox = element.$.cookiesCheckboxBasic;
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();
    await cookieCheckbox.$.checkbox.updateComplete;
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    await testBrowserProxy.whenCalled('clearBrowsingData');
    // Passing showPasswordsNotice = true should trigger the notice about
    // incomplete password deletions to open, and the dialog to stay open.
    promiseResolver.resolve(
        {showHistoryNotice: false, showPasswordsNotice: true});
    await promiseResolver.promise;

    // Yields to the message loop to allow the callback chain of the
    // Promise that was just resolved to execute before the
    // assertions.
    flush();
    const notice1 = element.shadowRoot!
                        .querySelector<SettingsPasswordsDeletionDialogElement>(
                            '#passwordsNotice');
    assertTrue(!!notice1);
    const noticeActionButton =
        notice1.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!noticeActionButton);

    // The notice should have replaced the main dialog.
    assertFalse(element.$.clearBrowsingDataDialog.open);
    assertTrue(notice1.$.dialog.open);

    const whenNoticeClosed = eventToPromise('close', notice1!);

    // Tapping the action button will close the notice.
    noticeActionButton.click();

    await whenNoticeClosed;
    const notice2 = element.shadowRoot!.querySelector('#passwordsNotice');
    assertFalse(!!notice2);
    assertFalse(element.$.clearBrowsingDataDialog.open);
  });

  test('showBothHistoryAndPasswordsDeletionDialog', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);
    const actionButton =
        element.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox = element.$.cookiesCheckboxBasic;
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();
    await cookieCheckbox.$.checkbox.updateComplete;
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    await testBrowserProxy.whenCalled('clearBrowsingData');
    // Passing showHistoryNotice = true and showPasswordsNotice = true
    // should first trigger the notice about other forms of browsing
    // history to open, then once that is acknowledged, the notice about
    // incomplete password deletions should open. The main CBD dialog
    // should stay open during that whole time.
    promiseResolver.resolve(
        {showHistoryNotice: true, showPasswordsNotice: true});
    await promiseResolver.promise;

    // Yields to the message loop to allow the callback chain of the
    // Promise that was just resolved to execute before the
    // assertions.
    flush();
    const notice1 =
        element.shadowRoot!.querySelector<SettingsHistoryDeletionDialogElement>(
            '#historyNotice');
    assertTrue(!!notice1);
    const noticeActionButton1 =
        notice1.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!noticeActionButton1);

    // The notice should have replaced the main dialog.
    assertFalse(element.$.clearBrowsingDataDialog.open);
    assertTrue(notice1.$.dialog.open);

    const whenNoticeClosed1 = eventToPromise('close', notice1!);

    // Tapping the action button will close the history notice, and
    // display the passwords notice instead.
    noticeActionButton1.click();

    await whenNoticeClosed1;
    // The passwords notice should have replaced the history notice.
    const historyNotice = element.shadowRoot!.querySelector('#historyNotice');
    assertFalse(!!historyNotice);
    const passwordsNotice =
        element.shadowRoot!.querySelector('#passwordsNotice');
    assertTrue(!!passwordsNotice);

    flush();
    const notice2 = element.shadowRoot!
                        .querySelector<SettingsPasswordsDeletionDialogElement>(
                            '#passwordsNotice');
    assertTrue(!!notice2);
    const noticeActionButton2 =
        notice2.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!noticeActionButton2);

    assertFalse(element.$.clearBrowsingDataDialog.open);
    assertTrue(notice2.$.dialog.open);

    const whenNoticeClosed2 = eventToPromise('close', notice2!);

    // Tapping the action button will close the notice.
    noticeActionButton2.click();

    await whenNoticeClosed2;
    const notice3 = element.shadowRoot!.querySelector('#passwordsNotice');
    assertFalse(!!notice3);
    assertFalse(element.$.clearBrowsingDataDialog.open);
  });

  test('Counters', function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);

    const checkbox = element.shadowRoot!.querySelector<SettingsCheckboxElement>(
        '#cacheCheckboxBasic');
    assertTrue(!!checkbox);
    assertEquals('browser.clear_data.cache_basic', checkbox.pref!.key);

    // Simulate a browsing data counter result for history. This checkbox's
    // sublabel should be updated.
    webUIListenerCallback('update-counter-text', checkbox.pref!.key, 'result');
    assertEquals('result', checkbox.subLabel);
  });

  // <if expr="is_chromeos">
  // On ChromeOS the footer is never shown.
  test('ClearBrowsingDataSyncAccountInfo', function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);

    // Not syncing.
    webUIListenerCallback('sync-status-changed', {
      signedIn: false,
      hasError: false,
    });
    flush();
    assertFalse(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));

    // Syncing.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: false,
    });
    flush();
    assertFalse(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));

    // Sync passphrase error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    flush();
    assertFalse(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));

    // Other sync error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(!!element.shadowRoot!.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
  });
  // </if>
});


suite('ClearBrowsingDataForSupervisedUsers', function() {
  let testBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let element: SettingsClearBrowsingDataDialogElement;

  setup(function() {
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    loadTimeData.overrideValues({
      isChildAccount: true,
    });
  });

  teardown(function() {
    element.remove();
  });

  test('history rows are shown for supervised users', async function() {
    document.body.appendChild(element);
    await testBrowserProxy.whenCalled('initialize');

    assertTrue(element.$.clearBrowsingDataDialog.open);
    assertFalse(element.shadowRoot!
                    .querySelector<SettingsCheckboxElement>(
                        '#browsingCheckbox')!.hidden);
    assertFalse(element.shadowRoot!
                    .querySelector<SettingsCheckboxElement>(
                        '#browsingCheckboxBasic')!.hidden);
    assertFalse(element.shadowRoot!
                    .querySelector<SettingsCheckboxElement>(
                        '#downloadCheckbox')!.hidden);
  });

  // <if expr="is_win or is_macosx or is_linux">
  test(
      'Additional information shown for supervised users when clearing cookies',
      async function() {
        document.body.appendChild(element);
        await testBrowserProxy.whenCalled('initialize');

        assertTrue(element.$.clearBrowsingDataDialog.open);

        // Supervised users will see additional text informing them they will
        // not be signed out when cookies are cleared
        const checkbox =
            element.shadowRoot!.querySelector<SettingsCheckboxElement>(
                '#cookiesCheckboxBasic')!;

        assertEquals(
            element.i18n('clearCookiesSummarySignedInSupervisedProfile')
                .toString(),
            checkbox.subLabel);
      });
  // </if>
});
