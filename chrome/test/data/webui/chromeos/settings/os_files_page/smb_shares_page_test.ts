// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AddSmbShareDialogElement, SettingsSmbSharesPageElement, SmbBrowserProxy, SmbBrowserProxyImpl, SmbMountResult} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrInputElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestSmbBrowserProxy extends TestBrowserProxy implements SmbBrowserProxy {
  smbMountResult = SmbMountResult.SUCCESS;
  constructor() {
    super([
      'smbMount',
      'startDiscovery',
    ]);
  }

  smbMount(
      smbUrl: string, smbName: string, username: string, password: string,
      authMethod: string, shouldOpenFileManagerAfterMount: boolean,
      saveCredentials: boolean): Promise<SmbMountResult> {
    this.methodCalled(
        'smbMount', smbUrl, smbName, username, password, authMethod,
        shouldOpenFileManagerAfterMount, saveCredentials);
    return Promise.resolve(this.smbMountResult);
  }

  startDiscovery(): void {
    this.methodCalled('startDiscovery');
  }

  updateCredentials(mountId: string, username: string, password: string): void {
    this.methodCalled('updateCredentials', mountId, username, password);
  }

  hasAnySmbMountedBefore(): Promise<boolean> {
    this.methodCalled('hasAnySmbMountedBefore');
    return Promise.resolve(true);
  }
}

suite('<settings-smb-shares-page>', () => {
  let page: SettingsSmbSharesPageElement;
  let addDialog: AddSmbShareDialogElement|null;
  let smbBrowserProxy: TestSmbBrowserProxy;
  let smbSuccessEventDispatched: boolean = false;

  setup(() => {
    smbBrowserProxy = new TestSmbBrowserProxy();
    SmbBrowserProxyImpl.setInstance(smbBrowserProxy);

    page = document.createElement('settings-smb-shares-page');
    document.body.appendChild(page);

    const button = page.shadowRoot!.querySelector<CrButtonElement>('#addShare');
    assert(button);
    button.click();
    flush();

    addDialog = page.shadowRoot!.querySelector('add-smb-share-dialog');
    assert(addDialog);

    flush();
  });

  teardown(() => {
    page.remove();
    addDialog = null;
  });

  test('AddBecomesEnabled', () => {
    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);

    url.value = 'smb://192.168.1.1/testshare';

    const addButton =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assert(addButton);
    assertFalse(addButton.disabled);
  });

  test('AddDisabledWithInvalidUrl', () => {
    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);
    const addButton =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assert(addButton);

    url.value = '';
    assertTrue(addButton.disabled);

    // Invalid scheme (must start with smb:// or \\)
    url.value = 'foobar';
    assertTrue(addButton.disabled);
    url.value = 'foo\\\\bar\\baz';
    assertTrue(addButton.disabled);
    url.value = 'smb:/foo/bar';
    assertTrue(addButton.disabled);

    // Incomplete (must be of the form \\server\share)
    url.value = '\\\\foo';
    assertTrue(addButton.disabled);
    url.value = '\\\\foo\\';
    assertTrue(addButton.disabled);
    url.value = '\\\\foo\\\\';
    assertTrue(addButton.disabled);

    // Incomplete (must be of the form smb://server/share)
    url.value = 'smb://';
    assertTrue(addButton.disabled);
    url.value = 'smb://foo';
    assertTrue(addButton.disabled);
    url.value = 'smb://foo/';
    assertTrue(addButton.disabled);

    // Valid URLs.
    url.value = '\\\\foo\\bar';
    assertFalse(addButton.disabled);
    url.value = 'smb://foo/bar';
    assertFalse(addButton.disabled);
  });

  test('ClickAdd', async () => {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';
    const expectedSmbName = 'testname';
    const expectedUsername = 'username';
    const expectedPassword = 'password';
    const expectedAuthMethod = 'credentials';
    const expectedShouldOpenFileManager = false;

    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);
    url.value = expectedSmbUrl;

    const name = addDialog.shadowRoot!.querySelector<CrInputElement>('#name');
    assert(name);
    name.value = expectedSmbName;

    const un = addDialog.shadowRoot!.querySelector<CrInputElement>('#username');
    assert(un);
    un.value = expectedUsername;

    const pw = addDialog.shadowRoot!.querySelector<CrInputElement>('#password');
    assert(pw);
    pw.value = expectedPassword;

    const addButton =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assert(addButton);

    addDialog.set('authenticationMethod_', expectedAuthMethod);
    addDialog.shouldOpenFileManagerAfterMount = expectedShouldOpenFileManager;

    smbBrowserProxy.resetResolver('smbMount');
    addButton.click();
    const smbMountArguments = await smbBrowserProxy.whenCalled('smbMount');
    assertEquals(expectedSmbUrl, smbMountArguments[0]);
    assertEquals(expectedSmbName, smbMountArguments[1]);
    assertEquals(expectedUsername, smbMountArguments[2]);
    assertEquals(expectedPassword, smbMountArguments[3]);
    assertEquals(expectedAuthMethod, smbMountArguments[4]);
    assertEquals(expectedShouldOpenFileManager, smbMountArguments[5]);
  });

  test('EventDispatchedAfterClickAddSuccessful', async () => {
    page.addEventListener('smb-successfully-mounted-once', () => {
      smbSuccessEventDispatched = true;
    });

    const expectedSmbUrl = 'smb://192.168.1.1/testshare';
    const expectedSmbName = 'testname';
    const expectedUsername = 'username';
    const expectedPassword = 'password';
    const expectedAuthMethod = 'credentials';
    const expectedShouldOpenFileManager = false;

    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);
    url.value = expectedSmbUrl;

    const name = addDialog.shadowRoot!.querySelector<CrInputElement>('#name');
    assert(name);
    name.value = expectedSmbName;

    const un = addDialog.shadowRoot!.querySelector<CrInputElement>('#username');
    assert(un);
    un.value = expectedUsername;

    const pw = addDialog.shadowRoot!.querySelector<CrInputElement>('#password');
    assert(pw);
    pw.value = expectedPassword;

    const addButton =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assert(addButton);

    addDialog.set('authenticationMethod_', expectedAuthMethod);
    addDialog.shouldOpenFileManagerAfterMount = expectedShouldOpenFileManager;

    smbBrowserProxy.resetResolver('smbMount');
    smbBrowserProxy.smbMountResult = SmbMountResult.SUCCESS;
    addButton.click();
    await smbBrowserProxy.whenCalled('smbMount');

    assertTrue(smbSuccessEventDispatched);
  });

  test('StartDiscovery', async () => {
    await smbBrowserProxy.whenCalled('startDiscovery');
  });

  test('ControlledByPolicy', () => {
    const button = page.shadowRoot!.querySelector<CrButtonElement>('#addShare');
    assert(button);

    assertEquals(
        null, page.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    assertFalse(button.disabled);

    page.prefs = {
      network_file_shares: {allowed: {value: false}},
    };
    flush();

    assert(page.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    assertTrue(button.disabled);
  });

  test('AuthenticationSelectorVisibility', () => {
    assert(addDialog);
    const authenticationMethod =
        addDialog.shadowRoot!.querySelector<HTMLElement>(
            '#authentication-method');
    assert(authenticationMethod);

    assertTrue(authenticationMethod.hidden);

    addDialog.set('isKerberosEnabled_', true);

    assertFalse(authenticationMethod.hidden);
  });

  test('AuthenticationSelectorControlsCredentialFields', () => {
    assert(addDialog);
    addDialog.set('isKerberosEnabled_', true);

    assertFalse(
        addDialog.shadowRoot!
            .querySelector<HTMLElement>('#authentication-method')!.hidden);

    const dropDown =
        addDialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
    assert(dropDown);

    const credentials =
        addDialog.shadowRoot!.querySelector<HTMLElement>('#credentials');
    assert(credentials);

    dropDown.value = 'kerberos';
    dropDown.dispatchEvent(new CustomEvent('change'));
    flush();

    assertTrue(credentials.hidden);

    dropDown.value = 'credentials';
    dropDown.dispatchEvent(new CustomEvent('change'));
    flush();

    assertFalse(credentials.hidden);
  });

  test('MostRecentlyUsedUrl', () => {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';

    page.remove();

    page = document.createElement('settings-smb-shares-page');
    page.prefs = {
      network_file_shares: {
        most_recently_used_url: {value: expectedSmbUrl},
        allowed: {value: true},
      },
    };
    document.body.appendChild(page);

    const button = page.shadowRoot!.querySelector<CrButtonElement>('#addShare');
    assert(button);
    assertFalse(button.disabled);
    button.click();

    flush();

    addDialog = page.shadowRoot!.querySelector('add-smb-share-dialog');
    assert(addDialog);

    flush();

    const openDialogButton =
        page.shadowRoot!.querySelector<CrButtonElement>('#addShare');
    openDialogButton!.click();

    assertEquals(expectedSmbUrl, addDialog.get('mountUrl_'));
    assertEquals(expectedSmbUrl, addDialog.get('mountUrl_'));
  });

  test('InvalidUrlErrorDisablesAddButton', async () => {
    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);
    const addButton =
        addDialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assert(addButton);

    // Invalid URL, but passes regex test.
    url.value = 'smb://foo\\\\/bar';
    assertFalse(addButton.disabled);

    smbBrowserProxy.smbMountResult = SmbMountResult.INVALID_URL;
    addButton.click();

    await new Promise((resolve) => {
      const pollFunc = () => {
        if (url.errorMessage && addButton.disabled) {
          resolve(null);
          return;
        }
        setTimeout(pollFunc, 100);
      };
      // url.errorMessage can't be observed for a change, so instead, poll.
      pollFunc();
    });
  });

  test('LoadingBarDuringDiscovery', async () => {
    assert(addDialog);
    const url = addDialog.shadowRoot!.querySelector('cr-searchable-drop-down');
    assert(url);
    // Loading bar is shown when the page loads.
    assertTrue(url.showLoading);

    await smbBrowserProxy.whenCalled('startDiscovery');

    webUIListenerCallback('on-shares-found', ['smb://foo/bar'], false);
    assertTrue(url.showLoading);

    webUIListenerCallback('on-shares-found', ['smb://foo/bar2'], true);
    assertFalse(url.showLoading);

    assertEquals(2, url.items.length);
  });
});
