// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var availableTests = [

  function testSendSyntheticKeyEvent() {
    let tabCount = 0;
    chrome.tabs.onCreated.addListener(function(tab) {
      tabCount++;
      if (tabCount == 2)
        chrome.test.succeed();
    });

    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: 'keydown',
      keyCode: 84 /* T */,
      modifiers: {
        ctrl: true
      }
    });

    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: 'keydown',
      keyCode: 84 /* T */,
      modifiers: {
        ctrl: true,
        alt: false,
        shift: false,
        search: false
      }
    });
  },

  function testGetDisplayNameForLocale() {
    // The implementation of getDisplayNameForLocale() is more heavily
    // unittested elsewhere; here, we just need a sanity check to make sure
    // everything is correctly wired up.
    chrome.test.assertEq(
        'English',
        chrome.accessibilityPrivate.getDisplayNameForLocale('en', 'en'));
    chrome.test.assertEq(
        'Cantonese (Hong Kong)',
        chrome.accessibilityPrivate.getDisplayNameForLocale('yue-HK', 'en'));
    chrome.test.succeed();
  },

  function testOpenSettingsSubpage() {
    chrome.accessibilityPrivate.openSettingsSubpage('manageAccessibility/tts');
    chrome.test.notifyPass();
  },

  function testOpenSettingsSubpageInvalidSubpage() {
    chrome.accessibilityPrivate.openSettingsSubpage('fakeSettingsPage');
    chrome.test.notifyPass();
  },

  function testFeatureDisabled() {
    chrome.accessibilityPrivate.isFeatureEnabled(
        'enhancedNetworkVoices', (enabled) => {
          chrome.test.assertFalse(enabled);
          chrome.test.succeed();
        });
  },

  function testFeatureEnabled() {
    chrome.accessibilityPrivate.isFeatureEnabled(
        'enhancedNetworkVoices', (enabled) => {
          chrome.test.assertTrue(enabled);
          chrome.test.succeed();
        });
  },

  function testFeatureUnknown() {
    try {
      chrome.accessibilityPrivate.isFeatureEnabled('fooBar', () => {});
      // Should throw error before this point.
      chrome.test.fail();
    } catch (err) {
      // Expect call to throw error.
      chrome.test.succeed();
    }
  },

  function testAcceptConfirmationDialog() {
    chrome.accessibilityPrivate.showConfirmationDialog(
        'Confirm me! 🐶', 'This dialog should be confirmed.', (confirmed) => {
      chrome.test.assertTrue(confirmed);
      chrome.test.succeed();
    });

    // Notify the C++ test that it can confirm the dialog box.
    chrome.test.notifyPass();
  },

  function testCancelConfirmationDialog() {
    chrome.accessibilityPrivate.showConfirmationDialog(
        'Cancel me!', 'This dialog should be canceled', (confirmed) => {
      chrome.test.assertFalse(confirmed);
      chrome.test.succeed();
    });

    // Notify the C++ test that it can cancel the dialog box.
    chrome.test.notifyPass();
  },

  function testUpdateDictationBubble() {
    const update = chrome.accessibilityPrivate.updateDictationBubble;
    const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;

    // The typical flow for this API is as follows:
    // 1. Show the UI with the standby icon.
    // 2. Update the UI with some speech results and hide all icons.
    // 3. If the speech results match a Dictation macro (and the macro ran
    // successfully), then show the macro succeeded icon along with the
    // recognized text.
    // 4. Reset the UI and show the standby icon.
    // 5. Hide the UI.
    update({visible: true, icon: IconType.STANDBY});
    chrome.test.sendMessage('Standby', (proceed) => {
      update({visible: true, icon: IconType.HIDDEN, text: 'Hello'});
      chrome.test.sendMessage('Show text', (proceed) => {
        update({visible: true, icon: IconType.MACRO_SUCCESS, text: 'Hello'});
        chrome.test.sendMessage('Show macro success', (proceed) => {
          update({visible: true, icon: IconType.STANDBY});
          chrome.test.sendMessage('Reset', (proceed) => {
            update({visible: false, icon: IconType.HIDDEN});
            chrome.test.sendMessage('Hide');
            chrome.test.succeed();
          });
        });
      });
    });

    chrome.test.notifyPass();
  },

  function testUpdateDictationBubbleWithHints() {
    const update = chrome.accessibilityPrivate.updateDictationBubble;
    const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;
    const HintType = chrome.accessibilityPrivate.DictationBubbleHintType;
    update({
      visible: true,
      icon: IconType.STANDBY,
      hints: [HintType.TRY_SAYING, HintType.TYPE, HintType.HELP]
    });
    chrome.test.sendMessage('Some hints', (proceed) => {
      update({visible: true, icon: IconType.STANDBY});
      chrome.test.sendMessage('No hints');
      chrome.test.succeed();
    });

    chrome.test.notifyPass();
  },
];

chrome.test.getConfig(function(config) {
  chrome.test.runTests(availableTests.filter(function(testFunc) {
    return testFunc.name == config.customArg;
  }));
});
