// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isSavingNewPasswordsBlockedByMandatoryPolicy} from 'chrome://password-manager/password_manager.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('PolicyUtilsTest', function() {
  function makeCredentialsEnableServicePref(
      value: boolean, enforcement?: chrome.settingsPrivate.Enforcement,
      controlledBy?: chrome.settingsPrivate.ControlledBy):
      chrome.settingsPrivate.PrefObject<boolean> {
    return {
      key: 'credentials_enable_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value,
      enforcement,
      controlledBy,
    };
  }

  test('blocked when mandatory user policy turns saving off', function() {
    assertTrue(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(
            false, chrome.settingsPrivate.Enforcement.ENFORCED,
            chrome.settingsPrivate.ControlledBy.USER_POLICY)));
  });

  test('blocked when mandatory device policy turns saving off', function() {
    assertTrue(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(
            false, chrome.settingsPrivate.Enforcement.ENFORCED,
            chrome.settingsPrivate.ControlledBy.DEVICE_POLICY)));
  });

  test('not blocked when mandatory policy turns saving on', function() {
    assertFalse(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(
            true, chrome.settingsPrivate.Enforcement.ENFORCED,
            chrome.settingsPrivate.ControlledBy.USER_POLICY)));
  });

  test('not blocked when recommended policy turns saving off', function() {
    assertFalse(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(
            false, chrome.settingsPrivate.Enforcement.RECOMMENDED,
            chrome.settingsPrivate.ControlledBy.USER_POLICY)));
  });

  test('not blocked when extension turns saving off', function() {
    assertFalse(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(
            false, chrome.settingsPrivate.Enforcement.ENFORCED,
            chrome.settingsPrivate.ControlledBy.EXTENSION)));
  });

  test('not blocked when user turns saving off', function() {
    assertFalse(isSavingNewPasswordsBlockedByMandatoryPolicy(
        makeCredentialsEnableServicePref(false)));
  });
});
