// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_quick_unlock', function() {
  let testElement = null;
  let quickUnlockPrivateApi = null;
  const QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;
  let fakeUma = null;

  /**
   * Returns if the element is visible.
   * @param {!Element} element
   */
  function isVisible(element) {
    while (element) {
      if (element.offsetWidth <= 0 || element.offsetHeight <= 0 ||
          element.hidden ||
          window.getComputedStyle(element).visibility == 'hidden') {
        return false;
      }

      element = element.parentElement;

      if (element) {
        // cr-dialog itself will always be 0x0. It's the inner native <dialog>
        // that has actual dimensions.
        // (The same about PIN-KEYBOARD.)
        if (element.tagName == 'CR-DIALOG') {
          element = element.getNative();
        } else if (element.tagName == 'PIN-KEYBOARD') {
          element = element.$.root;
        }
      }
    }

    return true;
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
    let childElement = testElement.$$(selector);
    if (!childElement && testElement.$.pinKeyboard) {
      childElement = testElement.$.pinKeyboard.$$(selector);
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

      setup(function() {
        PolymerTest.clearBody();

        quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
        fakeUma = new settings.FakeQuickUnlockUma();

        testElement = document.createElement(
            'settings-lock-screen-password-prompt-dialog');
        testElement.writeUma_ = fakeUma.recordProgress.bind(fakeUma);
        document.body.appendChild(testElement);

        passwordPromptDialog = getFromElement('#passwordPrompt');
        passwordPromptDialog.quickUnlockPrivate = quickUnlockPrivateApi;

        Polymer.dom.flush();

        passwordElement = passwordPromptDialog.$$('#passwordInput');
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
        const confirmButton = passwordPromptDialog.$$('#confirmButton');
        quickUnlockPrivateApi.accountPassword = 'bar';
        passwordElement.value = 'foo';
        Polymer.dom.flush();

        passwordPromptDialog.$$('cr-button[class="action-button"]').click();
        Polymer.dom.flush();

        assertEquals(0, passwordElement.inputElement.selectionStart);
        assertEquals(
            passwordElement.value.length,
            passwordElement.inputElement.selectionEnd);
        assertTrue(passwordElement.invalid);
        assertTrue(confirmButton.disabled);

        // Changing value should reset invalid state.
        passwordElement.value = 'bar';
        Polymer.dom.flush();
        assertFalse(passwordElement.invalid);
        assertFalse(confirmButton.disabled);
      });

      test('TapConfirmButtonWithWrongPasswordRestoresFocus', function() {
        const confirmButton = passwordPromptDialog.$$('#confirmButton');
        quickUnlockPrivateApi.accountPassword = 'bar';
        passwordElement.value = 'foo';
        passwordPromptDialog.$$('cr-button[class="action-button"]').click();

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
        assertFalse(!!testElement.setModes);
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
        assertTrue(!!testElement.setModes);
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
        const confirmButton = passwordPromptDialog.$$('#confirmButton');
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
          if (radioButton == element) {
            assertTrue(element.checked, 'Expected ' + name + ' to be checked');
          } else {
            assertFalse(
                element.checked, 'Expected ' + name + ' to be unchecked');
          }
        }

        doAssert(passwordRadioButton, 'passwordButton');
        doAssert(pinPasswordRadioButton, 'pinPasswordButton');
      }

      /**
       * Returns the lock screen pref value.
       * @return {boolean}
       */
      function getLockScreenPref() {
        let result;
        fakeSettings.getPref(ENABLE_LOCK_SCREEN_PREF, function(value) {
          result = value;
        });
        assertNotEquals(undefined, result);
        return result.value;
      }

      /**
       * Changes the lock screen pref value using the settings API; this is like
       * the pref got changed from an unknown source such as another tab.
       * @param {boolean} value
       */
      function setLockScreenPref(value) {
        fakeSettings.setPref(ENABLE_LOCK_SCREEN_PREF, value, '', assertTrue);
      }

      function isSetupPinButtonVisible() {
        Polymer.dom.flush();
        const setupPinButton = testElement.$$('#setupPinButton');
        return isVisible(setupPinButton);
      }

      setup(function() {
        PolymerTest.clearBody();

        CrSettingsPrefs.deferInitialization = true;

        // Build pref fakes.
        const fakePrefs = [
          {
            key: ENABLE_LOCK_SCREEN_PREF,
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true
          },
          {
            key: 'ash.message_center.lock_screen_mode',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: 'hide'
          }
        ];
        fakeSettings = new settings.FakeSettingsPrivate(fakePrefs);
        fakeUma = new settings.FakeQuickUnlockUma();
        setLockScreenPref(true);
        const prefElement = document.createElement('settings-prefs');
        prefElement.initialize(fakeSettings);
        document.body.appendChild(prefElement);

        // Wait for prefElement to finish initializing; it takes some time for
        // the prefs element to get allocated.
        return test_util.eventToPromise('prefs-changed', prefElement)
            .then(function() {
              quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();

              // Create choose-method element.
              testElement = document.createElement('settings-lock-screen');
              testElement.settingsPrivate_ = fakeSettings;
              testElement.quickUnlockPrivate = quickUnlockPrivateApi;
              testElement.prefs = prefElement.prefs;
              testElement.writeUma_ = fakeUma.recordProgress.bind(fakeUma);

              document.body.appendChild(testElement);
              Polymer.dom.flush();

              testElement.setModes_ = quickUnlockPrivateApi.setModes.bind(
                  quickUnlockPrivateApi, quickUnlockPrivateApi.getFakeToken(),
                  [], [], () => {
                    return true;
                  });

              return test_util.waitBeforeNextRender(testElement);
            })
            .then(() => {
              passwordRadioButton =
                  getFromElement('cr-radio-button[name="password"]');
              pinPasswordRadioButton =
                  getFromElement('cr-radio-button[name="pin+password"]');
            });
      });

      // Showing the choose method screen does not make any destructive pref or
      // quickUnlockPrivate calls.
      test('ShowingScreenDoesNotModifyPrefs', function() {
        assertTrue(getLockScreenPref());
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
        assertEquals(
            quickUnlockPrivateApi.lockScreenEnabled, !lockScreenEnabled);

        toggle.click();
        assertEquals(toggle.checked, lockScreenEnabled);
        assertEquals(
            quickUnlockPrivateApi.lockScreenEnabled, lockScreenEnabled);
      });

      // The various radio buttons update internal state and do not modify
      // prefs.
      test('TappingButtonsChangesUnderlyingState', function() {
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

        // Verify toggling PIN on/off does not disable screen lock.
        setLockScreenPref(true);
        togglePin();
        assertTrue(getLockScreenPref());

        // Verify toggling PIN on/off does not enable screen lock.
        setLockScreenPref(false);
        togglePin();
        assertFalse(getLockScreenPref());
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

      // Tapping the PIN configure button opens up the setup PIN dialog, and
      // records a chose pin or password uma.
      test('TappingConfigureOpensSetupPin', function() {
        assertEquals(
            0,
            fakeUma.getHistogramValue(
                LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
        assertRadioButtonChecked(passwordRadioButton);

        pinPasswordRadioButton.click();
        assertTrue(isSetupPinButtonVisible());
        assertRadioButtonChecked(pinPasswordRadioButton);

        Polymer.dom.flush();
        getFromElement('#setupPinButton').click();
        Polymer.dom.flush();
        const setupPinDialog = getFromElement('#setupPin');
        assertTrue(setupPinDialog.$$('#dialog').open);
        assertEquals(
            1,
            fakeUma.getHistogramValue(
                LockScreenProgress.CHOOSE_PIN_OR_PASSWORD));
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

        quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
        fakeUma = new settings.FakeQuickUnlockUma();

        // Create setup-pin element.
        testElement = document.createElement('settings-setup-pin-dialog');
        testElement.quickUnlockPrivate = quickUnlockPrivateApi;
        document.body.appendChild(testElement);

        const testPinKeyboard = testElement.$.pinKeyboard;
        testPinKeyboard.setModes = (modes, credentials, onComplete) => {
          quickUnlockPrivateApi.setModes(
              quickUnlockPrivateApi.getFakeToken(), modes, credentials, () => {
                onComplete(true);
              });
        };
        testPinKeyboard.writeUma = fakeUma.recordProgress.bind(fakeUma);

        Polymer.dom.flush();

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
        assertFalse(testElement.$$('#dialog').open);
      });

      // Hitting cancel at the confirm step dismisses the dialog.
      test('HittingBackButtonResetsState', function() {
        pinKeyboard.value = '1111';
        continueButton.click();
        backButton.click();
        assertFalse(testElement.$$('#dialog').open);
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
        assertEquals(
            0, fakeUma.getHistogramValue(LockScreenProgress.ENTER_PIN));
        assertEquals(
            0, fakeUma.getHistogramValue(LockScreenProgress.CONFIRM_PIN));
        pinKeyboard.value = '1111';
        continueButton.click();
        assertEquals(
            1, fakeUma.getHistogramValue(LockScreenProgress.ENTER_PIN));

        pinKeyboard.value = '1111';
        continueButton.click();

        assertEquals(
            1, fakeUma.getHistogramValue(LockScreenProgress.CONFIRM_PIN));
        assertDeepEquals(['PIN'], quickUnlockPrivateApi.activeModes);
        assertDeepEquals(['1111'], quickUnlockPrivateApi.credentials);
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
        const backspaceButton = pinKeyboard.$$('#backspaceButton');
        assertTrue(!!backspaceButton);
        assertTrue(backspaceButton.disabled);

        pinKeyboard.$$('cr-input').value = '11';
        assertFalse(backspaceButton.disabled);

        pinKeyboard.$$('cr-input').value = '';
        assertTrue(backspaceButton.disabled);
      });
    });
  }

  return {
    registerAuthenticateTests: registerAuthenticateTests,
    registerLockScreenTests: registerLockScreenTests,
    registerSetupPinDialogTests: registerSetupPinDialogTests
  };
});
