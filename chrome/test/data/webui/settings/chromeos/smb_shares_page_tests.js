// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SmbBrowserProxyImpl, SmbMountResult} from 'chrome://os-settings/chromeos/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {smb_shares.SmbBrowserProxy} */
class TestSmbBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'smbMount',
      'startDiscovery',
    ]);
    /** @type{!SmbMountResult} */
    this.smbMountResult = SmbMountResult.SUCCESS;
  }

  /** @override */
  smbMount(smbUrl, smbName, username, password, authMethod, inSettings) {
    this.methodCalled(
        'smbMount',
        [smbUrl, smbName, username, password, authMethod, inSettings]);
    return Promise.resolve(this.smbMountResult);
  }

  /** @override */
  startDiscovery() {
    this.methodCalled('startDiscovery');
  }
}

suite('AddSmbShareDialogTests', function() {
  let page = null;
  let addDialog = null;

  /** @type {?smb_shares.TestSmbBrowserProxy} */
  let smbBrowserProxy = null;

  setup(function() {
    smbBrowserProxy = new TestSmbBrowserProxy();
    SmbBrowserProxyImpl.instance_ = smbBrowserProxy;

    PolymerTest.clearBody();

    page = document.createElement('settings-smb-shares-page');
    document.body.appendChild(page);

    const button = page.shadowRoot.querySelector('#addShare');
    assertTrue(!!button);
    button.click();
    flush();

    addDialog = page.shadowRoot.querySelector('add-smb-share-dialog');
    assertTrue(!!addDialog);

    flush();
  });

  teardown(function() {
    page.remove();
    addDialog = null;
    page = null;
  });

  test('AddBecomesEnabled', function() {
    const url = addDialog.$.address;

    assertTrue(!!url);
    url.value = 'smb://192.168.1.1/testshare';

    const addButton = addDialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  });

  test('AddDisabledWithInvalidUrl', function() {
    const url = addDialog.$.address;
    const addButton = addDialog.shadowRoot.querySelector('.action-button');

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

  test('ClickAdd', function() {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';
    const expectedSmbName = 'testname';
    const expectedUsername = 'username';
    const expectedPassword = 'password';
    const expectedAuthMethod = 'credentials';
    const expectedShouldOpenFileManager = false;

    const url = addDialog.shadowRoot.querySelector('#address');
    assertTrue(!!url);
    url.value = expectedSmbUrl;

    const name = addDialog.shadowRoot.querySelector('#name');
    assertTrue(!!name);
    name.value = expectedSmbName;

    const un = addDialog.shadowRoot.querySelector('#username');
    assertTrue(!!un);
    un.value = expectedUsername;

    const pw = addDialog.shadowRoot.querySelector('#password');
    assertTrue(!!pw);
    pw.value = expectedPassword;

    const addButton = addDialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!addButton);

    addDialog.authenticationMethod_ = expectedAuthMethod;
    addDialog.shouldOpenFileManagerAfterMount = expectedShouldOpenFileManager;

    smbBrowserProxy.resetResolver('smbMount');
    addButton.click();
    return smbBrowserProxy.whenCalled('smbMount').then(function(args) {
      assertEquals(expectedSmbUrl, args[0]);
      assertEquals(expectedSmbName, args[1]);
      assertEquals(expectedUsername, args[2]);
      assertEquals(expectedPassword, args[3]);
      assertEquals(expectedAuthMethod, args[4]);
      assertEquals(expectedShouldOpenFileManager, args[5]);
    });
  });

  test('StartDiscovery', function() {
    return smbBrowserProxy.whenCalled('startDiscovery');
  });

  test('ControlledByPolicy', function() {
    const button = page.shadowRoot.querySelector('#addShare');

    assertFalse(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertFalse(button.disabled);

    page.prefs = {
      network_file_shares: {allowed: {value: false}},
    };
    flush();

    assertTrue(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertTrue(button.disabled);
  });

  test('AuthenticationSelectorVisibility', function() {
    const authenticationMethod =
        addDialog.shadowRoot.querySelector('#authentication-method');
    assertTrue(!!authenticationMethod);

    assertTrue(authenticationMethod.hidden);

    addDialog.isActiveDirectory_ = true;

    assertFalse(authenticationMethod.hidden);
  });

  test('AuthenticationSelectorControlsCredentialFields', function() {
    addDialog.isActiveDirectory_ = true;

    assertFalse(
        addDialog.shadowRoot.querySelector('#authentication-method').hidden);

    const dropDown = addDialog.shadowRoot.querySelector('.md-select');
    assertTrue(!!dropDown);

    const credentials = addDialog.shadowRoot.querySelector('#credentials');
    assertTrue(!!credentials);

    dropDown.value = 'kerberos';
    dropDown.dispatchEvent(new CustomEvent('change'));
    flush();

    assertTrue(credentials.hidden);

    dropDown.value = 'credentials';
    dropDown.dispatchEvent(new CustomEvent('change'));
    flush();

    assertFalse(credentials.hidden);
  });

  test('MostRecentlyUsedUrl', function() {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';

    PolymerTest.clearBody();

    page = document.createElement('settings-smb-shares-page');
    page.prefs = {
      network_file_shares: {
        most_recently_used_url: {value: expectedSmbUrl},
        allowed: {value: true},
      },
    };
    document.body.appendChild(page);

    const button = page.shadowRoot.querySelector('#addShare');
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();

    flush();

    addDialog = page.shadowRoot.querySelector('add-smb-share-dialog');
    assertTrue(!!addDialog);

    flush();

    const openDialogButton = page.shadowRoot.querySelector('#addShare');
    openDialogButton.click();

    assertEquals(expectedSmbUrl, addDialog.mountUrl_);
    assertEquals(expectedSmbUrl, addDialog.mountUrl_);
  });

  test('InvalidUrlErrorDisablesAddButton', function() {
    const url = addDialog.$.address;
    const addButton = addDialog.shadowRoot.querySelector('.action-button');

    // Invalid URL, but passes regex test.
    url.value = 'smb://foo\\\\/bar';
    assertFalse(addButton.disabled);

    smbBrowserProxy.smbMountResult = SmbMountResult.INVALID_URL;
    addButton.click();

    return new Promise((resolve, reject) => {
      const pollFunc = () => {
        if (url.errorMessage && addButton.disabled) {
          resolve();
          return;
        }
        setTimeout(pollFunc, 100);
      };
      // url.errorMessage can't be observed for a change, so instead, poll.
      pollFunc();
    });
  });

  test('LoadingBarDuringDiscovery', async function() {
    const url = addDialog.$.address;
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
