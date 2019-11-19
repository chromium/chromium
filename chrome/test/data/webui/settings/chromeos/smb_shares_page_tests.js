// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    smb_shares.SmbBrowserProxyImpl.instance_ = smbBrowserProxy;

    PolymerTest.clearBody();

    page = document.createElement('settings-smb-shares-page');
    document.body.appendChild(page);

    const button = page.$$('#addShare');
    assertTrue(!!button);
    button.click();
    Polymer.dom.flush();

    addDialog = page.$$('add-smb-share-dialog');
    assertTrue(!!addDialog);

    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
    addDialog = null;
    page = null;
  });

  test('AddBecomesEnabled', function() {
    const url = addDialog.$.address;

    expectTrue(!!url);
    url.value = 'smb://192.168.1.1/testshare';

    const addButton = addDialog.$$('.action-button');
    expectTrue(!!addButton);
    expectFalse(addButton.disabled);
  });

  test('AddDisabledWithInvalidUrl', function() {
    const url = addDialog.$.address;
    const addButton = addDialog.$$('.action-button');

    url.value = '';
    expectTrue(addButton.disabled);

    // Invalid scheme (must start with smb:// or \\)
    url.value = 'foobar';
    expectTrue(addButton.disabled);
    url.value = 'foo\\\\bar\\baz';
    expectTrue(addButton.disabled);
    url.value = 'smb:/foo/bar';
    expectTrue(addButton.disabled);

    // Incomplete (must be of the form \\server\share)
    url.value = '\\\\foo';
    expectTrue(addButton.disabled);
    url.value = '\\\\foo\\';
    expectTrue(addButton.disabled);
    url.value = '\\\\foo\\\\';
    expectTrue(addButton.disabled);

    // Incomplete (must be of the form smb://server/share)
    url.value = 'smb://';
    expectTrue(addButton.disabled);
    url.value = 'smb://foo';
    expectTrue(addButton.disabled);
    url.value = 'smb://foo/';
    expectTrue(addButton.disabled);

    // Valid URLs.
    url.value = '\\\\foo\\bar';
    expectFalse(addButton.disabled);
    url.value = 'smb://foo/bar';
    expectFalse(addButton.disabled);
  });

  test('ClickAdd', function() {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';
    const expectedSmbName = 'testname';
    const expectedUsername = 'username';
    const expectedPassword = 'password';
    const expectedAuthMethod = 'credentials';
    const expectedShouldOpenFileManager = false;

    const url = addDialog.$$('#address');
    expectTrue(!!url);
    url.value = expectedSmbUrl;

    const name = addDialog.$$('#name');
    expectTrue(!!name);
    name.value = expectedSmbName;

    const un = addDialog.$$('#username');
    expectTrue(!!un);
    un.value = expectedUsername;

    const pw = addDialog.$$('#password');
    expectTrue(!!pw);
    pw.value = expectedPassword;

    const addButton = addDialog.$$('.action-button');
    expectTrue(!!addButton);

    addDialog.authenticationMethod_ = expectedAuthMethod;
    addDialog.shouldOpenFileManagerAfterMount = expectedShouldOpenFileManager;

    smbBrowserProxy.resetResolver('smbMount');
    addButton.click();
    return smbBrowserProxy.whenCalled('smbMount').then(function(args) {
      expectEquals(expectedSmbUrl, args[0]);
      expectEquals(expectedSmbName, args[1]);
      expectEquals(expectedUsername, args[2]);
      expectEquals(expectedPassword, args[3]);
      expectEquals(expectedAuthMethod, args[4]);
      expectEquals(expectedShouldOpenFileManager, args[5]);
    });
  });

  test('StartDiscovery', function() {
    return smbBrowserProxy.whenCalled('startDiscovery');
  });

  test('ControlledByPolicy', function() {
    const button = page.$$('#addShare');

    assertFalse(!!page.$$('cr-policy-pref-indicator'));
    expectFalse(button.disabled);

    page.prefs = {
      network_file_shares: {allowed: {value: false}},
    };
    Polymer.dom.flush();

    assertTrue(!!page.$$('cr-policy-pref-indicator'));
    assertTrue(button.disabled);
  });

  test('AuthenticationSelectorVisibility', function() {
    const authenticationMethod = addDialog.$$('#authentication-method');
    expectTrue(!!authenticationMethod);

    expectTrue(authenticationMethod.hidden);

    addDialog.isActiveDirectory_ = true;

    expectFalse(authenticationMethod.hidden);
  });

  test('AuthenticationSelectorControlsCredentialFields', function() {
    addDialog.isActiveDirectory_ = true;

    expectFalse(addDialog.$$('#authentication-method').hidden);

    const dropDown = addDialog.$$('.md-select');
    expectTrue(!!dropDown);

    const credentials = addDialog.$$('#credentials');
    expectTrue(!!credentials);

    dropDown.value = 'kerberos';
    dropDown.dispatchEvent(new CustomEvent('change'));
    Polymer.dom.flush();

    expectTrue(credentials.hidden);

    dropDown.value = 'credentials';
    dropDown.dispatchEvent(new CustomEvent('change'));
    Polymer.dom.flush();

    expectFalse(credentials.hidden);
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

    const button = page.$$('#addShare');
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();

    Polymer.dom.flush();

    addDialog = page.$$('add-smb-share-dialog');
    assertTrue(!!addDialog);

    Polymer.dom.flush();

    const openDialogButton = page.$$('#addShare');
    openDialogButton.click();

    expectEquals(expectedSmbUrl, addDialog.mountUrl_);
    expectEquals(expectedSmbUrl, addDialog.mountUrl_);
  });

  test('InvalidUrlErrorDisablesAddButton', function() {
    const url = addDialog.$.address;
    const addButton = addDialog.$$('.action-button');

    // Invalid URL, but passes regex test.
    url.value = 'smb://foo\\\\/bar';
    expectFalse(addButton.disabled);

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
    expectTrue(url.showLoading);

    await smbBrowserProxy.whenCalled('startDiscovery');

    cr.webUIListenerCallback('on-shares-found', ['smb://foo/bar'], false);
    expectTrue(url.showLoading);

    cr.webUIListenerCallback('on-shares-found', ['smb://foo/bar2'], true);
    expectFalse(url.showLoading);

    expectEquals(2, url.items.length);
  });

});
