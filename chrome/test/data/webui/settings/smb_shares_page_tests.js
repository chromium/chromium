// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.SmbBrowserProxy} */
class TestSmbBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'smbMount',
      'startDiscovery',
    ]);
  }

  /** @override */
  smbMount(smbUrl, smbName, username, password, authMethod) {
    this.methodCalled(
        'smbMount', [smbUrl, smbName, username, password, authMethod]);
  }

  /** @override */
  startDiscovery() {
    this.methodCalled('startDiscovery');
  }
}

suite('AddSmbShareDialogTests', function() {
  let page = null;
  let addDialog = null;

  /** @type {?settings.TestSmbBrowserProxy} */
  let smbBrowserProxy = null;

  setup(function() {
    smbBrowserProxy = new TestSmbBrowserProxy();
    settings.SmbBrowserProxyImpl.instance_ = smbBrowserProxy;

    PolymerTest.clearBody();

    page = document.createElement('settings-smb-shares-page');
    document.body.appendChild(page);

    const button = page.$$('#addShare');
    assertTrue(!!button);
    button.click();
    Polymer.dom.flush();

    addDialog = page.$$('settings-add-smb-share-dialog');
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

    const addButton = addDialog.$$('#actionButton');
    expectTrue(!!addButton);
    expectFalse(addButton.disabled);
  });

  test('ClickAdd', function() {
    const expectedSmbUrl = 'smb://192.168.1.1/testshare';
    const expectedSmbName = 'testname';
    const expectedUsername = 'username';
    const expectedPassword = 'password';
    const expectedAuthMethod = 'credentials';

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

    addButton.click();
    return smbBrowserProxy.whenCalled('smbMount').then(function(args) {
      expectEquals(expectedSmbUrl, args[0]);
      expectEquals(expectedSmbName, args[1]);
      expectEquals(expectedUsername, args[2]);
      expectEquals(expectedPassword, args[3]);
      expectEquals(expectedAuthMethod, args[4]);
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

});
