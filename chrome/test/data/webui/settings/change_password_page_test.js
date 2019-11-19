// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.ChangePasswordBrowserProxy} */
class TestChangePasswordBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'changePassword',
    ]);
  }

  /** @override */
  changePassword() {
    this.methodCalled('changePassword');
  }
}

suite('ChangePasswordHandler', function() {
  let changePasswordPage = null;

  /** @type {?TestChangePasswordBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestChangePasswordBrowserProxy();
    settings.ChangePasswordBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    changePasswordPage =
        document.createElement('settings-change-password-page');
    document.body.appendChild(changePasswordPage);
  });

  teardown(function() {
    changePasswordPage.remove();
  });

  test('changePasswordButtonPressed', function() {
    const actionButton = changePasswordPage.$$('#changePassword');
    assertTrue(!!actionButton);
    actionButton.click();
    return browserProxy.whenCalled('changePassword');
  });
});
