// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.quickUnlockPrivate for testing.
 */

import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';

/**
 * A couple weak pins to use for testing.
 */
const TEST_WEAK_PINS = ['1111', '1234', '1313', '2001', '1010'];

const FAKE_TOKEN = 'token';

function clearError() {
  chrome.runtime.lastError = undefined;
}

const CredentialProblem = chrome.quickUnlockPrivate.CredentialProblem;
const QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;

type CredentialRequirements = chrome.quickUnlockPrivate.CredentialRequirements;
type QuickUnlockPrivateApi = typeof chrome.quickUnlockPrivate;
type QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;
type CredentialCheck = chrome.quickUnlockPrivate.CredentialCheck;
type TokenInfo = chrome.quickUnlockPrivate.TokenInfo;

/**
 * Fake of the chrome.quickUnlockPrivate API.
 */
export class FakeQuickUnlockPrivate implements QuickUnlockPrivateApi {
  // Mirroring chrome.languageSettingsPrivate API member.
  /* eslint-disable @typescript-eslint/naming-convention */
  QuickUnlockMode = QuickUnlockMode;
  CredentialProblem = CredentialProblem;
  /* eslint-enable @typescript-eslint/naming-convention */

  accountPassword = '';
  activeModes: QuickUnlockMode[] = [];
  availableModes: QuickUnlockMode[] = [QuickUnlockMode.PIN];
  credentials: string[] = [];
  credentialRequirements: CredentialRequirements = {minLength: 4, maxLength: 0};
  lockScreenEnabled = false;
  pinAutosubmitEnabled = false;
  pinAuthenticationPossible = true;
  onActiveModesChanged = new FakeChromeEvent();

  // Public testing methods.
  getFakeToken(): TokenInfo {
    return {token: FAKE_TOKEN, lifetimeSeconds: 0};
  }

  // Public fake API implementations.
  getAuthToken(password: string, onComplete: (info: TokenInfo) => void): void {
    if (password !== this.accountPassword) {
      chrome.runtime.lastError = {message: 'Incorrect Password'};
      return;
    }

    clearError();
    onComplete(this.getFakeToken());
  }

  setLockScreenEnabled(
      token: string, enabled: boolean, onComplete?: () => void): void {
    if (token !== FAKE_TOKEN) {
      chrome.runtime.lastError = {message: 'Authentication token invalid'};
    } else {
      // Note: Fake does not set pref value.
      this.lockScreenEnabled = enabled;
      clearError();
    }

    if (onComplete) {
      onComplete();
    }
  }

  setPinAutosubmitEnabled(
      token: string, pin: string, enabled: boolean,
      onComplete?: (success: boolean) => void): void {
    if (token !== FAKE_TOKEN) {
      chrome.runtime.lastError = {message: 'Authentication token invalid'};
    } else {
      this.pinAutosubmitEnabled = enabled && this.credentials[0] === pin;
      clearError();
    }

    if (onComplete) {
      // Successful if disabling, or enabling with the correct pin.
      const success = !enabled || this.pinAutosubmitEnabled;
      onComplete(success);
    }
  }

  canAuthenticatePin(onComplete?: (success: boolean) => void): void {
    if (onComplete) {
      onComplete(this.pinAuthenticationPossible);
    }
  }

  getAvailableModes(onComplete: (modes: QuickUnlockMode[]) => void): void {
    clearError();
    onComplete(this.availableModes);
  }

  getActiveModes(onComplete: (modes: QuickUnlockMode[]) => void): void {
    clearError();
    onComplete(this.activeModes);
  }

  setModes(
      token: string, modes: QuickUnlockMode[], credentials: string[],
      onComplete: () => void): void {
    if (token !== FAKE_TOKEN) {
      chrome.runtime.lastError = {message: 'Authentication token invalid'};
      onComplete();
      return;
    }
    this.activeModes = modes;
    this.credentials = credentials;
    clearError();
    onComplete();
  }

  checkCredential(
      _mode: QuickUnlockMode, credential: string,
      onComplete: (credentialCheck: CredentialCheck) => void): void {
    const message: CredentialCheck = {
      errors: [],
      warnings: [],
    };

    if (!!credential &&
        credential.length < this.credentialRequirements.minLength) {
      message.errors.push(CredentialProblem.TOO_SHORT);
    }

    if (!!credential && this.credentialRequirements.maxLength !== 0 &&
        credential.length > this.credentialRequirements.maxLength) {
      message.errors.push(CredentialProblem.TOO_LONG);
    }

    if (!!credential && TEST_WEAK_PINS.includes(credential)) {
      message.warnings.push(CredentialProblem.TOO_WEAK);
    }

    clearError();
    onComplete(message);
  }

  getCredentialRequirements(
      _mode: QuickUnlockMode,
      onComplete: (requirements: CredentialRequirements) => void): void {
    clearError();
    onComplete(this.credentialRequirements);
  }
}
