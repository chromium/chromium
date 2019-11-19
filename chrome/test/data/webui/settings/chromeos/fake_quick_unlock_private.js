// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.quickUnlockPrivate for testing.
 */

/**
 * A couple weak pins to use for testing.
 * @const
 */
const TEST_WEAK_PINS = ['1111', '1234', '1313', '2001', '1010'];

const FAKE_TOKEN = 'token';

cr.define('settings', function() {
  /**
   * Fake of the chrome.quickUnlockPrivate API.
   * @constructor
   * @implements {QuickUnlockPrivate}
   */
  function FakeQuickUnlockPrivate() {
    /** @type {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} */
    this.availableModes = [chrome.quickUnlockPrivate.QuickUnlockMode.PIN];
    /** @type {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} */
    this.activeModes = [];
    /** @type {!Array<string>} */ this.credentials = [];
    /** @type {string} */ this.accountPassword = '';
    /** @type {!chrome.quickUnlockPrivate.CredentialRequirements} */
    this.credentialRequirements = {minLength: 4, maxLength: 0};
    /** @type {boolean} */ this.lockScreenEnabled = false;
  }

  function clearError_() {
    chrome.runtime.lastError = undefined;
  }

  FakeQuickUnlockPrivate.prototype = {
    // Public testing methods.
    getFakeToken: function() {
      return FAKE_TOKEN;
    },

    // Public fake API implementations.
    /**
     * @override
     * @param {function(
     *     !Array<!chrome.quickUnlockPrivate.QuickUnlockMode>):void} onComplete
     */
    getAuthToken: function(password, onComplete) {
      if (password != this.accountPassword) {
        chrome.runtime.lastError = 'Incorrect Password';
        onComplete();
        return;
      }
      clearError_();
      onComplete({token: FAKE_TOKEN, lifetime: 0});
    },

    /**
     * @override
     * @param {string} token
     * @param {boolean} enabled
     * @param {function(boolean):void}= onComplete
     */
    setLockScreenEnabled: function(token, enabled, onComplete) {
      if (token != FAKE_TOKEN) {
        chrome.runtime.lastError = 'Authentication token invalid';
      } else {
        // Note: Fake does not set pref value.
        this.lockScreenEnabled = enabled;
        clearError_();
      }
      if (onComplete) {
        onComplete();
      }
    },

    /**
     * @override
     * @param {function(
     *     !Array<!chrome.quickUnlockPrivate.QuickUnlockMode>):void} onComplete
     */
    getAvailableModes: function(onComplete) {
      clearError_();
      onComplete(this.availableModes);
    },

    /**
     * @override
     * @param {function(
     *     !Array<!chrome.quickUnlockPrivate.QuickUnlockMode>):void} onComplete
     */
    getActiveModes: function(onComplete) {
      clearError_();
      onComplete(this.activeModes);
    },

    /**
     * @override
     * @param {string} token
     * @param {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} modes
     * @param {!Array<string>} credentials
     * @param {function(boolean):void} onComplete
     */
    setModes: function(token, modes, credentials, onComplete) {
      if (token != FAKE_TOKEN) {
        chrome.runtime.lastError = 'Authentication token invalid';
        onComplete();
        return;
      }
      this.activeModes = modes;
      this.credentials = credentials;
      clearError_();
      onComplete();
    },

    /**
     * @override
     * @param {!chrome.quickUnlockPrivate.QuickUnlockMode} mode
     * @param {string} credential
     * @param {function(
     *     !chrome.quickUnlockPrivate.CredentialCheck):void} onComplete
     */
    checkCredential: function(mode, credential, onComplete) {
      const message = {};
      const errors = [];
      const warnings = [];

      if (!!credential &&
          credential.length < this.credentialRequirements.minLength) {
        errors.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_SHORT);
      }

      if (!!credential && this.credentialRequirements.maxLength != 0 &&
          credential.length > this.credentialRequirements.maxLength) {
        errors.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_LONG);
      }

      if (!!credential && TEST_WEAK_PINS.includes(credential)) {
        warnings.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_WEAK);
      }

      message.errors = errors;
      message.warnings = warnings;
      clearError_();
      onComplete(message);
    },

    /**
     * @override.
     * @param {!chrome.quickUnlockPrivate.QuickUnlockMode} mode
     * @param {function(
     *     !chrome.quickUnlockPrivate.CredentialRequirements):void onComplete
     */
    getCredentialRequirements: function(mode, onComplete) {
      clearError_();
      onComplete(this.credentialRequirements);
    },
  };

  /** @type {!ChromeEvent} */
  FakeQuickUnlockPrivate.prototype.onActiveModesChanged = new FakeChromeEvent();

  return {FakeQuickUnlockPrivate: FakeQuickUnlockPrivate};
});
