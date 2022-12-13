// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {LockScreenProgress} from 'chrome://resources/ash/common/quick_unlock/lock_screen_constants.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/settings/chromeos/fake_settings_private.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible as testUtilIsVisible} from 'chrome://webui-test/test_util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.js';
import {FakeQuickUnlockUma} from './fake_quick_unlock_uma.js';

let testElement = null;
let quickUnlockPrivateApi = null;
const QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;
let fakeUma = null;

/**
 * Returns if the element is visible.
 * @param {!Element} element
 */
function isVisible(element) {
  return testUtilIsVisible(element) &&
      window.getComputedStyle(element).visibility !== 'hidden';
}

/**
 * Returns true if the given |element| has class |className|.
 * @param {!Element} element
 * @param {string} className
 */
function assertHasClass(element, className) {
  assertTrue(element.classList.contains(className));
}

/**
 * Returns the result of running |selector| on testElement.
 * @param {string} selector
 * @return {Element}
 */
function getFromElement(selector) {
  let childElement = testElement.shadowRoot.querySelector(selector);
  if (!childElement && testElement.$.pinKeyboard) {
    childElement = testElement.$.pinKeyboard.shadowRoot.querySelector(selector);
  }

  assertTrue(!!childElement);
  return childElement;
}

/**
 * Sets the active quick unlock modes and raises a mode change event.
 * @param {!Array<chrome.quickUnlockPrivate.QuickUnlockMode>} modes
 */
function setActiveModes(modes) {
  quickUnlockPrivateApi.activeModes = modes;
  quickUnlockPrivateApi.onActiveModesChanged.callListeners(modes);
}

function registerAuthenticateTests() {
  suite('authenticate', function() {
    let passwordPromptDialog = null;
    let passwordElement = null;
    let authTokenObtainedFired = false;

    setup(function() {
      PolymerTest.clearBody();

      quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
      fakeUma = new FakeQuickUnlockUma();

      testElement =
          document.createElement('settings-lock-screen-password-prompt-dialog');
      testElement.writeUma_ = fakeUma.recordProgress.bind(fakeUma);
      testElement.addEventListener('auth-token-obtained', (e) => {
        authTokenObtainedFired = true;
      });
      document.body.appendChild(testElement);

      passwordPromptDialog = getFromElement('#passwordPrompt');
      passwordPromptDialog.quickUnlockPrivate = quickUnlockPrivateApi;

      flush();

      passwordElement =
          passwordPromptDialog.shadowRoot.querySelector('#passwordInput');
    });

    test('PasswordCheckDoesNotChangeActiveMode', function() {
      // No active modes.
      quickUnlockPrivateApi.activeModes = [];
      passwordElement.value = 'foo';
      passwordPromptDialog.submitPassword_();
      assertDeepEquals([], quickUnlockPrivateApi.activeModes);
      assertDeepEquals([], quickUnlockPrivateApi.credentials);

      // PIN is active.
      quickUnlockPrivateApi.activeModes = [QuickUnlockMode.PIN];
      passwordElement.value = 'foo';
      passwordPromptDialog.submitPassword_();
      assertDeepEquals(
          [QuickUnlockMode.PIN], quickUnlockPrivateApi.activeModes);
      assertDeepEquals([], quickUnlockPrivateApi.credentials);
    });

    test('InvalidPasswordInteractions', function() {
      const confirmButton =
          passwordPromptDialog.shadowRoot.querySelector('#confirmButton');
      quickUnlockPrivateApi.accountPassword = 'bar';
      passwordElement.value = 'foo';
      flush();

      passwordPromptDialog.shadowRoot
          .querySelector('cr-button[class="action-button"]')
          .click();
      flush();

      assertEquals(0, passwordElement.inputElement.selectionStart);
      assertEquals(
          passwordElement.value.length,
          passwordElement.inputElement.selectionEnd);
      assertTrue(passwordElement.invalid);
      assertTrue(confirmButton.disabled);

      // Changing value should reset invalid state.
      passwordElement.value = 'bar';
      flush();
      assertFalse(passwordElement.invalid);
      assertFalse(confirmButton.disabled);
    });

    test('TapConfirmButtonWithWrongPasswordRestoresFocus', function() {
      const confirmButton =
          passwordPromptDialog.shadowRoot.querySelector('#confirmButton');
      quickUnlockPrivateApi.accountPassword = 'bar';
      passwordElement.value = 'foo';
      passwordPromptDialog.shadowRoot
          .querySelector('cr-button[class="action-button"]')
          .click();

      assertTrue(passwordElement.hasAttribute('focused_'));
    });

    // A bad password does not provide an authenticated setModes object, and a
    // entered password correctly uma should not be recorded.
    test('InvalidPasswordDoesNotProvideAuthentication', function() {
      quickUnlockPrivateApi.accountPassword = 'bar';

      passwordElement.value = 'foo';
      passwordPromptDialog.submitPassword_();

      assertEquals(
          0,
          fakeUma.getHistogramValue(
              LockScreenProgress.ENTER_PASSWORD_CORRECTLY));
      assertFalse(authTokenObtainedFired);
    });

    // A valid password provides an authenticated setModes object, and a
    // entered password correctly uma should be recorded.
    test('ValidPasswordProvidesAuthentication', function() {
      quickUnlockPrivateApi.accountPassword = 'foo';

      passwordElement.value = 'foo';
      passwordPromptDialog.submitPassword_();

      assertEquals(
          1,
          fakeUma.getHistogramValue(
              LockScreenProgress.ENTER_PASSWORD_CORRECTLY));
      assertTrue(authTokenObtainedFired);
    });

    // The setModes objects times out after a delay.
    test('AuthenticationTimesOut', function(done) {
      quickUnlockPrivateApi.accountPassword = 'foo';

      passwordElement.value = 'foo';
      passwordPromptDialog.submitPassword_();
      // Fake lifetime is 0 so setModes should be reset in next frame.
      setTimeout(function() {
        assertFalse(!!testElement.setModes);
        done();
      }, 0);
    });

    test('ConfirmButtonDisabledWhenEmpty', function() {
      // Confirm button is diabled when there is nothing entered.
      const confirmButton =
          passwordPromptDialog.shadowRoot.querySelector('#confirmButton');
      assertTrue(!!confirmButton);
      assertTrue(confirmButton.disabled);

      passwordElement.value = 'foo';
      assertFalse(!!confirmButton.disabled);
      passwordElement.value = '';
      assertTrue(confirmButton.disabled);
    });
  });
}

function registerLockScreenTests() {
  suite('lock-screen', function() {
    const ENABLE_LOCK_SCREEN_PREF = 'settings.enable_screen_lock';
    const ENABLE_PIN_AUTOSUBMIT_PREF = 'pin_unlock_autosubmit_enabled';

    let fakeSettings = null;
    let passwordRadioButton = null;
    let pinPasswordRadioButton = null;
    const noneRadioButton = null;

    /**
     * Asserts that only the given radio button is checked and all of the
     * others are unchecked.
     * @param {Element} radioButton
     */
    function assertRadioButtonChecked(radioButton) {
      function doAssert(element, name) {
        if (radioButton === element) {
          assertTrue(element.checked, 'Expected ' + name + ' to be checked');
        } else {
          assertFalse(element.checked, 'Expected ' + name + ' to be unchecked');
        }
      }

      doAssert(passwordRadioButton, 'passwordButton');
      doAssert(pinPasswordRadioButton, 'pinPasswordButton');
    }

    /**
     * Returns the lock screen pref value.
     * @return {!Promise<boolean>}
     */
    function getLockScreenPref() {
      return fakeSettings.getPref(ENABLE_LOCK_SCREEN_PREF).then((result) => {
        assertNotEquals(undefined, result);
        return result.value;
      });
    }

    /**
     * Changes the lock screen pref value using the settings API; this is like
     * the pref got changed from an unknown source such as another tab.
     * @param {boolean} value
     * @return {!Promise<void>}
     */
    function setLockScreenPref(value) {
      return fakeSettings.setPref(ENABLE_LOCK_SCREEN_PREF, value, '')
          .then(assertTrue);
    }

    function isSetupPinButtonVisible() {
      flush();
      const setupPinButton =
          testElement.shadowRoot.querySelector('#setupPinButton');
      return isVisible(setupPinButton);
    }

    function isEnablePinAutosubmitToggleVisible() {
      flush();
      const autosubmitToggle =
          testElement.shadowRoot.querySelector('#enablePinAutoSubmit');
      return autosubmitToggle && isVisible(autosubmitToggle);
    }

    setup(function() {
      PolymerTest.clearBody();

      CrSettingsPrefs.deferInitialization = true;

      // Build pref fakes.
      const fakePrefs = [
        {
          key: ENABLE_LOCK_SCREEN_PREF,
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        {
          key: 'ash.message_center.lock_screen_mode',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'hide',
        },
        {
          key: ENABLE_PIN_AUTOSUBMIT_PREF,
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      ];
      fakeSettings = new FakeSettingsPrivate(fakePrefs);
      fakeUma = new FakeQuickUnlockUma();
      setLockScreenPref(true);
      const prefElement = document.createElement('settings-prefs');
      prefElement.initialize(fakeSettings);
      document.body.appendChild(prefElement);

      // Wait for prefElement to finish initializing; it takes some time for
      // the prefs element to get allocated.
      return eventToPromise('prefs-changed', prefElement)
          .then(function() {
            quickUnlockPrivateApi = new FakeQuickUnlockPrivate();

            // Create choose-method element.
            testElement = document.createElement('settings-lock-screen');
            testElement.settingsPrivate_ = fakeSettings;
            testElement.quickUnlockPrivate = quickUnlockPrivateApi;
            testElement.prefs = prefElement.prefs;
            testElement.writeUma_ = fakeUma.recordProgress.bind(fakeUma);

            document.body.appendChild(testElement);
            flush();

            testElement.setModes = quickUnlockPrivateApi.setModes.bind(
                quickUnlockPrivateApi,
                quickUnlockPrivateApi.getFakeToken().token, [], [], () => {
                  return true;
                });

            return waitBeforeNextRender(testElement);
          })
          .then(() => {
            passwordRadioButton =
                getFromElement('cr-radio-button[name="password"]');
            pinPasswordRadioButton =
                getFromElement('cr-radio-button[name="pin+password"]');
          });
    });

    teardown(function() {
      Router.getInstance().resetRouteForTesting();
    });

    // Showing the choose method screen does not make any destructive pref or
    // quickUnlockPrivate calls.
    test('ShowingScreenDoesNotModifyPrefs', async function() {
      assertTrue(await getLockScreenPref());
      assertRadioButtonChecked(passwordRadioButton);
      assertDeepEquals([], quickUnlockPrivateApi.activeModes);
    });

    // Toggling the lock screen preference calls setLockScreenEnabled.
    test('SetLockScreenEnabled', function() {
      testElement.authToken = quickUnlockPrivateApi.getFakeToken();
      const toggle = getFromElement('#enableLockScreen');
      const lockScreenEnabled = toggle.checked;
      quickUnlockPrivateApi.lockScreenEnabled = lockScreenEnabled;

      toggle.click();
      assertEquals(toggle.checked, !lockScreenEnabled);
      assertEquals(quickUnlockPrivateApi.lockScreenEnabled, !lockScreenEnabled);

      toggle.click();
      assertEquals(toggle.checked, lockScreenEnabled);
      assertEquals(quickUnlockPrivateApi.lockScreenEnabled, lockScreenEnabled);
    });

    test('Deep link to enable lock screen', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '1109');
      Router.getInstance().navigateTo(routes.LOCK_SCREEN, params);

      const deepLinkElement = getFromElement('#enableLockScreen')
                                  .shadowRoot.querySelector('cr-toggle');
      assert(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Lock screen toggle should be focused for settingId=1109.');
    });

    // The various radio buttons update internal state and do not modify
    // prefs.
    test('TappingButtonsChangesUnderlyingState', async function() {
      function togglePin() {
        assertRadioButtonChecked(passwordRadioButton);

        // Tap pin+password button.
        pinPasswordRadioButton.click();
        assertRadioButtonChecked(pinPasswordRadioButton);
        assertTrue(isSetupPinButtonVisible());
        assertDeepEquals([], quickUnlockPrivateApi.activeModes);

        // Enable quick unlock so that we verify tapping password disables it.
        setActiveModes([QuickUnlockMode.PIN]);

        // Tap password button and verify quick unlock is disabled.
        passwordRadioButton.click();
        assertRadioButtonChecked(passwordRadioButton);
        assertFalse(isSetupPinButtonVisible());
        assertDeepEquals([], quickUnlockPrivateApi.activeModes);
      }
      testElement.authToken = quickUnlockPrivateApi.getFakeToken();

      // Verify toggling PIN on/off does not disable screen lock.
      await setLockScreenPref(true);
      togglePin();
      assertTrue(await getLockScreenPref());

      // Verify toggling PIN on/off does not enable screen lock.
      await setLockScreenPref(false);
      togglePin();
      assertFalse(await getLockScreenPref());
    });

    // If quick unlock is changed by another settings page the radio button
    // will update to show quick unlock is active.
    test('EnablingQuickUnlockChangesButtonState', function() {
      setActiveModes([QuickUnlockMode.PIN]);
      assertRadioButtonChecked(pinPasswordRadioButton);
      assertTrue(isSetupPinButtonVisible());

      setActiveModes([]);
      assertRadioButtonChecked(passwordRadioButton);
      assertDeepEquals([], quickUnlockPrivateApi.activeModes);
    });

    // Tests correct UI conflict resolution in the event of a race condition
    // that may occur when:
    // (1) User selects PIN_PASSSWORD, and successfully sets a pin, adding
    //     QuickUnlockMode.PIN to active modes.
    // (2) User selects PASSWORD, QuickUnlockMode.PIN capability is cleared
    //     from the active modes, notifying LockStateBehavior to call
    //     updateUnlockType to fetch the active modes asynchronously.
    // (3) User selects PIN_PASSWORD, but the process from step 2 has
    //     not yet completed.
    // See https://crbug.com/1054327 for details.
    test('UserSelectsPinBeforePasswordOnlyStateSet', function() {
      setActiveModes([QuickUnlockMode.PIN]);
      assertRadioButtonChecked(pinPasswordRadioButton);
      assertTrue(isSetupPinButtonVisible());
      flush();
      assertEquals(
          testElement.shadowRoot.querySelector('#setupPinButton').innerText,
          'Change PIN');

      // Clicking will trigger an async call which setActiveModes([]) fakes.
      passwordRadioButton.click();
      assertFalse(isSetupPinButtonVisible());

      pinPasswordRadioButton.click();
      assertTrue(isSetupPinButtonVisible());

      // Simulate the state change to PASSWORD after selecting PIN radio.
      setActiveModes([]);

      flush();
      assertRadioButtonChecked(pinPasswordRadioButton);
      assertTrue(isSetupPinButtonVisible());
      assertEquals(
          testElement.shadowRoot.querySelector('#setupPinButton').innerText,
          'Set up PIN');
    });

    // Tapping the PIN configure button opens up the setup PIN dialog, and
    // records a chose pin or password uma.
    test('TappingConfigureOpensSetupPin', function() {
      assertEquals(
          0,
          fakeUma.getHistogramValue(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
      assertRadioButtonChecked(passwordRadioButton);

      pinPasswordRadioButton.click();
      assertTrue(isSetupPinButtonVisible());
      assertRadioButtonChecked(pinPasswordRadioButton);

      flush();
      getFromElement('#setupPinButton').click();
      flush();
      const setupPinDialog = getFromElement('#setupPin');
      assertTrue(setupPinDialog.shadowRoot.querySelector('#dialog').open);
      assertEquals(
          1,
          fakeUma.getHistogramValue(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
    });

    test('TappingEnableAutoSubmitPinOpensDialog', function() {
      testElement.authToken = quickUnlockPrivateApi.getFakeToken();
      // No PIN is set yet.
      assertFalse(testElement.hasPin);
      testElement.hasPin = true;
      // Must be visible when there is a PIN set.
      assertTrue(isEnablePinAutosubmitToggleVisible());

      getFromElement('#enablePinAutoSubmit').click();
      flush();
      const autosubmitDialog = getFromElement('#pinAutosubmitDialog');
      assertTrue(autosubmitDialog.shadowRoot.querySelector('#dialog').open);

      // Cancel button closes the dialog.
      autosubmitDialog.shadowRoot.querySelector('#cancelButton').click();
      assertFalse(autosubmitDialog.shadowRoot.querySelector('#dialog').open);
    });
  });
}

function registerAutosubmitDialogTests() {
  suite('autosubmit-dialog', function() {
    let confirmButton = null;
    let cancelButton = null;
    let pinKeyboard = null;
    let errorDiv = null;
    let errorMsg = null;

    setup(function() {
      PolymerTest.clearBody();
      quickUnlockPrivateApi = new FakeQuickUnlockPrivate();

      // Create auto submit dialog.
      testElement = document.createElement('settings-pin-autosubmit-dialog');
      testElement.quickUnlockPrivate = quickUnlockPrivateApi;
      testElement.authToken = quickUnlockPrivateApi.getFakeToken();
      document.body.appendChild(testElement);
      flush();

      // Prepare the quick unlock private API.
      quickUnlockPrivateApi.credentials[0] = '123456';
      assertFalse(quickUnlockPrivateApi.pinAutosubmitEnabled);

      // Get the elements.
      pinKeyboard = getFromElement('#pinKeyboard');
      errorDiv = getFromElement('#errorDiv');
      cancelButton = getFromElement('#cancelButton');
      confirmButton = getFromElement('#confirmButton');
      errorMsg = getFromElement('#errorMessage');

      assertTrue(isVisible(cancelButton));
      assertTrue(isVisible(confirmButton));
    });

    test('WrongPinShowsError', function() {
      assertFalse(isVisible(errorDiv));
      pinKeyboard.value = '1234';
      assertFalse(confirmButton.disabled);
      confirmButton.click();
      assertFalse(quickUnlockPrivateApi.pinAutosubmitEnabled);
      assertTrue(isVisible(errorDiv));
      assertEquals(errorMsg.innerText, 'Incorrect PIN');
      assertTrue(confirmButton.disabled);
    });

    // PINs longer than 12 digits are not supported and cannot activate
    // auto submit.
    test('LongPinShowsError', function() {
      assertFalse(isVisible(errorDiv));
      pinKeyboard.value = '123456789012';  // 12 digits still ok
      assertFalse(confirmButton.disabled);
      pinKeyboard.value = '1234567890123';  // 13 digits - Not ok
      assertTrue(confirmButton.disabled);
      assertTrue(isVisible(errorDiv));
      assertEquals(
          errorMsg.innerText,
          'PIN must be 12 digits or less to use automatic unlock');
    });

    test('RightPinActivatesAutosubmit', function() {
      pinKeyboard.value = '123456';
      assertFalse(confirmButton.disabled);
      confirmButton.click();
      assertTrue(quickUnlockPrivateApi.pinAutosubmitEnabled);
    });

    // Tests that the dialog fires an event to invalidate the auth token
    // to trigger a password prompt.
    test('FireInvalidateTokenRequestWhenPinAuthNotPossible', async () => {
      // Simulate too many wrong PIN attempts.
      quickUnlockPrivateApi.pinAuthenticationPossible = false;
      pinKeyboard.value = '1234';
      const invalidateTokenEvent =
          eventToPromise('invalidate-auth-token-requested', testElement);
      confirmButton.click();
      await invalidateTokenEvent;
    });
  });
}

function registerSetupPinDialogTests() {
  suite('setup-pin-dialog', function() {
    let titleDiv = null;
    let problemDiv = null;
    let pinKeyboard = null;
    let pinInput = null;
    let backButton = null;
    let continueButton = null;

    setup(function() {
      PolymerTest.clearBody();

      quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
      fakeUma = new FakeQuickUnlockUma();

      // Create setup-pin element.
      testElement = document.createElement('settings-setup-pin-dialog');
      testElement.quickUnlockPrivate = quickUnlockPrivateApi;
      document.body.appendChild(testElement);

      const testPinKeyboard = testElement.$.pinKeyboard;
      testPinKeyboard.setModes = (modes, credentials, onComplete) => {
        quickUnlockPrivateApi.setModes(
            quickUnlockPrivateApi.getFakeToken().token, modes, credentials,
            () => {
              onComplete(true);
            });
      };
      testPinKeyboard.writeUma = fakeUma.recordProgress.bind(fakeUma);

      flush();

      titleDiv = getFromElement('div[slot=title]');
      problemDiv = getFromElement('#problemDiv');
      pinKeyboard = getFromElement('pin-keyboard');
      pinInput = pinKeyboard.$.pinInput;
      backButton = getFromElement('cr-button[class="cancel-button"]');
      continueButton = getFromElement('cr-button[class="action-button"]');

      assertTrue(isVisible(backButton));
      assertTrue(isVisible(continueButton));
    });

    test('Text input blocked', () => {
      const event = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'a', keyCode: 65});
      pinInput.dispatchEvent(event);
      assertTrue(event.defaultPrevented);
    });

    test('Numeric input not blocked', () => {
      const event = new KeyboardEvent(
          'keydown', {cancelable: true, key: '1', keyCode: 49});
      pinInput.dispatchEvent(event);
      assertFalse(event.defaultPrevented);
    });

    test('System keys not blocked', () => {
      const event = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'BrightnessUp', keyCode: 217});
      pinInput.dispatchEvent(event);
      assertFalse(event.defaultPrevented);
    });

    // The continue button and title change text between the setup and confirm
    // steps.
    test('TextChangesBetweenSetupAndConfirmStep', function() {
      const initialContinue = continueButton.textContent;
      const initialTitle = titleDiv.textContent;

      pinKeyboard.value = '1111';
      continueButton.click();

      assertNotEquals(initialContinue, continueButton.textContent);
      assertNotEquals(initialTitle, titleDiv.textContent);
    });

    // The continue button is disabled unless the user has entered a >= 4
    // digit PIN.
    test('CanOnlyContinueAfterEnteringAtLeastFourDigitPin', function() {
      pinKeyboard.value = '111';
      assertTrue(continueButton.disabled);

      pinKeyboard.value = '1111';
      assertFalse(continueButton.disabled);

      pinKeyboard.value = '111';
      assertTrue(continueButton.disabled);

      pinKeyboard.value = '';
      assertTrue(continueButton.disabled);

      pinKeyboard.value = '1111111';
      assertFalse(continueButton.disabled);
    });

    // Problem message is always shown.
    test('ProblemShownEvenWithEmptyPin', function() {
      pinKeyboard.value = '11';
      assertTrue(isVisible(problemDiv));

      pinKeyboard.value = '';
      assertTrue(isVisible(problemDiv));
    });

    // If the PIN is too short a problem is shown.
    test('WarningShownForShortPins', function() {
      // Verify initially when the PIN is less than 4 digits, the problem will
      // be a warning.
      pinKeyboard.value = '';
      assertTrue(isVisible(problemDiv));
      assertHasClass(problemDiv, 'warning');
      assertTrue(continueButton.disabled);

      // Verify that once the PIN is 4 digits (do not use 1111 since that will
      // bring up a easy to guess warning) the warning disappears.
      pinKeyboard.value = '1321';
      assertFalse(isVisible(problemDiv));
      assertFalse(continueButton.disabled);

      // Verify that after we pass 4 digits once, but delete some digits, the
      // problem will be an error.
      pinKeyboard.value = '11';
      assertHasClass(problemDiv, 'error');
      assertTrue(continueButton.disabled);
    });

    // If the PIN is too long an error problem is shown.
    test('WarningShownForLongPins', function() {
      // By default, there is no max length on pins.
      quickUnlockPrivateApi.credentialRequirements.maxLength = 5;

      // A pin of length five should be valid when max length is five.
      pinKeyboard.value = '11111';
      assertFalse(isVisible(problemDiv));
      assertFalse(continueButton.disabled);

      // A pin of length six should not be valid when max length is five.
      pinKeyboard.value = '111111';
      assertTrue(isVisible(problemDiv));
      assertHasClass(problemDiv, 'error');
      assertTrue(continueButton.disabled);
    });

    // If the PIN is weak a warning problem is shown.
    test('WarningShownForWeakPins', function() {
      pinKeyboard.value = '1111';

      assertTrue(isVisible(problemDiv));
      assertHasClass(problemDiv, 'warning');
    });

    // Show a error if the user tries to submit a PIN that does not match the
    // initial PIN. The error disappears once the user edits the wrong PIN.
    test('WarningThenErrorShownForMismatchedPins', function() {
      pinKeyboard.value = '1118';
      continueButton.click();

      // Entering a mismatched PIN shows a warning.
      pinKeyboard.value = '1119';
      assertFalse(isVisible(problemDiv));

      // Submitting a mismatched PIN shows an error. Directly call the button
      // event since a tap on the disabled button does nothing.
      testElement.onPinSubmit_();
      assertHasClass(problemDiv, 'error');

      // Changing the PIN changes the error to a warning.
      pinKeyboard.value = '111';
      assertFalse(isVisible(problemDiv));
    });

    // Hitting cancel at the setup step dismisses the dialog.
    test('HittingBackButtonResetsState', function() {
      backButton.click();
      assertFalse(testElement.shadowRoot.querySelector('#dialog').open);
    });

    // Hitting cancel at the confirm step dismisses the dialog.
    test('HittingBackButtonResetsState', function() {
      pinKeyboard.value = '1111';
      continueButton.click();
      backButton.click();
      assertFalse(testElement.shadowRoot.querySelector('#dialog').open);
    });

    // User has to re-enter PIN for confirm step.
    test('PinKeyboardIsResetForConfirmStep', function() {
      pinKeyboard.value = '1111';
      continueButton.click();
      assertEquals('', pinKeyboard.value);
    });

    // Completing the flow results in a call to the quick unlock private API.
    // Check that uma stats are called as expected.
    test('SubmittingPinCallsQuickUnlockApi', function() {
      // Entering the same (even weak) pin twice calls the quick unlock API
      // and sets up a PIN.
      assertEquals(0, fakeUma.getHistogramValue(LockScreenProgress.ENTER_PIN));
      assertEquals(
          0, fakeUma.getHistogramValue(LockScreenProgress.CONFIRM_PIN));
      pinKeyboard.value = '1111';
      continueButton.click();
      assertEquals(1, fakeUma.getHistogramValue(LockScreenProgress.ENTER_PIN));

      pinKeyboard.value = '1111';
      continueButton.click();

      assertEquals(
          1, fakeUma.getHistogramValue(LockScreenProgress.CONFIRM_PIN));
      assertDeepEquals(['PIN'], quickUnlockPrivateApi.activeModes);
      assertDeepEquals(['1111'], quickUnlockPrivateApi.credentials);
    });

    // Submitting a new pin disables the 'Confirm' continue button and the pin
    // field until the asynchronous update completes.
    test('SubmittingPinDisablesConfirmButtonAndPinInput', function() {
      pinKeyboard.value = '1111';
      continueButton.click();
      pinKeyboard.value = '1111';
      assertFalse(continueButton.disabled);

      let isPinInputDisabled = pinInput.disabled;
      assertFalse(isPinInputDisabled);

      return new Promise(resolve => {
        getFromElement('setup-pin-keyboard')
            .addEventListener('is-set-modes-call-pending_-changed', function() {
              assertNotEquals(isPinInputDisabled, pinInput.disabled);
              isPinInputDisabled = pinInput.disabled;
              resolve();
            });

        continueButton.click();
        assertTrue(continueButton.disabled);
      });
    });

    test('TestContinueButtonState', function() {
      pinKeyboard.value = '1111';
      continueButton.click();

      // Verify the button is disabled when we first enter the confirm step,
      // since the PIN value is empty.
      assertEquals('', pinKeyboard.value);
      assertTrue(continueButton.disabled);

      // Verify the button is enabled after we enter one digit.
      pinKeyboard.value = '1';
      assertFalse(continueButton.disabled);

      // Verify the button is disabled after we try to submit a wrong PIN.
      continueButton.click();
      assertTrue(continueButton.disabled);

      // Verify the button is enabled after we enter one digit again.
      pinKeyboard.value = '11';
      assertFalse(continueButton.disabled);
    });

    // Verify that the backspace button is disabled when there is nothing
    // entered.
    test('BackspaceDisabledWhenNothingEntered', function() {
      const backspaceButton =
          pinKeyboard.shadowRoot.querySelector('#backspaceButton');
      assertTrue(!!backspaceButton);
      assertTrue(backspaceButton.disabled);

      pinKeyboard.shadowRoot.querySelector('cr-input').value = '11';
      assertFalse(backspaceButton.disabled);

      pinKeyboard.shadowRoot.querySelector('cr-input').value = '';
      assertTrue(backspaceButton.disabled);
    });
  });
}

registerLockScreenTests();
registerAuthenticateTests();
registerSetupPinDialogTests();
registerAutosubmitDialogTests();
