// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var availableTests = [

  function testSendSyntheticKeyEvent() {
    let tabCount = 0;
    chrome.tabs.onCreated.addListener((tab) => {
      tabCount++;
      if (tabCount == 2) {
        chrome.test.succeed();
      }
    });

    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type: 'keydown', keyCode: 84 /* T */, modifiers: {ctrl: true}});

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

  async function testFeatureDisabled() {
    const enabled = await chrome.accessibilityPrivate.isFeatureEnabled(
        'dictationContextChecking');
    chrome.test.assertFalse(enabled);
    chrome.test.succeed();
  },

  async function testFeatureEnabled() {
    const enabled = await chrome.accessibilityPrivate.isFeatureEnabled(
        'dictationContextChecking');
    chrome.test.assertTrue(enabled);
    chrome.test.succeed();
  },

  async function testFeatureUnknown() {
    try {
      const enabled =
          await chrome.accessibilityPrivate.isFeatureEnabled('fooBar');
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

  async function testUpdateDictationBubble() {
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
    chrome.test.notifyPass();
    update({visible: true, icon: IconType.STANDBY});
    await chrome.test.sendMessage('Standby');
    update({visible: true, icon: IconType.HIDDEN, text: 'Hello'});
    await chrome.test.sendMessage('Show text');
    update({visible: true, icon: IconType.MACRO_SUCCESS, text: 'Hello'});
    await chrome.test.sendMessage('Show macro success');
    update({visible: true, icon: IconType.STANDBY});
    await chrome.test.sendMessage('Reset');
    update({visible: false, icon: IconType.HIDDEN});
    await chrome.test.sendMessage('Hide');
    chrome.test.succeed();
  },

  async function testUpdateDictationBubbleWithHints() {
    const update = chrome.accessibilityPrivate.updateDictationBubble;
    const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;
    const HintType = chrome.accessibilityPrivate.DictationBubbleHintType;

    chrome.test.notifyPass();
    update({
      visible: true,
      icon: IconType.STANDBY,
      hints: [HintType.TRY_SAYING, HintType.TYPE, HintType.HELP]
    });
    await chrome.test.sendMessage('Some hints');
    update({visible: true, icon: IconType.STANDBY});
    await chrome.test.sendMessage('No hints');
    chrome.test.succeed();
  },

  function testInstallPumpkinForDictationFail() {
    const error = `Couldn't retrieve Pumpkin data.`;
    chrome.accessibilityPrivate.installPumpkinForDictation(() => {
      chrome.test.assertLastError(error);
      chrome.test.succeed();
    });
  },

  async function testInstallPumpkinForDictationSuccess() {
    const data = await chrome.accessibilityPrivate.installPumpkinForDictation();
    chrome.test.assertTrue(Boolean(data));
    chrome.test.assertTrue(Object.keys(data).length === 13);
    for (const [key, value] of Object.entries(data)) {
      const fileContents = new TextDecoder().decode(value);
      switch (key) {
        case 'js_pumpkin_tagger_bin_js':
          chrome.test.assertEq('Fake js pumpkin tagger', fileContents);
          break;
        case 'tagger_wasm_main_js':
          chrome.test.assertEq('Fake tagger wasm js', fileContents);
          break;
        case 'tagger_wasm_main_wasm':
          chrome.test.assertEq('Fake tagger wasm wasm', fileContents);
          break;
        case 'en_us_action_config_binarypb':
          chrome.test.assertEq('Fake en_us action config', fileContents);
          break;
        case 'en_us_pumpkin_config_binarypb':
          chrome.test.assertEq('Fake en_us pumpkin config', fileContents);
          break;
        case 'fr_fr_action_config_binarypb':
          chrome.test.assertEq('Fake fr_fr action config', fileContents);
          break;
        case 'fr_fr_pumpkin_config_binarypb':
          chrome.test.assertEq('Fake fr_fr pumpkin config', fileContents);
          break;
        case 'it_it_action_config_binarypb':
          chrome.test.assertEq('Fake it_it action config', fileContents);
          break;
        case 'it_it_pumpkin_config_binarypb':
          chrome.test.assertEq('Fake it_it pumpkin config', fileContents);
          break;
        case 'de_de_action_config_binarypb':
          chrome.test.assertEq('Fake de_de action config', fileContents);
          break;
        case 'de_de_pumpkin_config_binarypb':
          chrome.test.assertEq('Fake de_de pumpkin config', fileContents);
          break;
        case 'es_es_action_config_binarypb':
          chrome.test.assertEq('Fake es_es action config', fileContents);
          break;
        case 'es_es_pumpkin_config_binarypb':
          chrome.test.assertEq('Fake es_es pumpkin config', fileContents);
          break;
        default:
          chrome.test.fail();
      }
    }
    chrome.test.succeed();
  },

  function testGetDlcContentsDlcNotOnDevice() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const error = 'Error: TTS language pack with locale is not installed: ' +
        'es-us';
    chrome.accessibilityPrivate.getDlcContents(ttsDlc, () => {
      chrome.test.assertLastError(error);
      chrome.test.succeed();
    });
  },

  async function testGetDlcContentsSuccess() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const contents = await chrome.accessibilityPrivate.getDlcContents(ttsDlc);
    chrome.test.assertNoLastError();
    chrome.test.assertEq(
        'Fake DLC file content', new TextDecoder().decode(contents));
    chrome.test.succeed();
  },

  function testGetTtsDlcContentsDlcNotOnDevice() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const ttsVariant = chrome.accessibilityPrivate.TtsVariant.LITE;
    const error = 'Error: TTS language pack with locale is not installed: ' +
        'es-us';
    chrome.accessibilityPrivate.getTtsDlcContents(ttsDlc, ttsVariant, () => {
      chrome.test.assertLastError(error);
      chrome.test.succeed();
    });
  },

  async function testGetTtsDlcContentsSuccess() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const ttsVariant = chrome.accessibilityPrivate.TtsVariant.LITE;
    const contents =
        await chrome.accessibilityPrivate.getTtsDlcContents(ttsDlc, ttsVariant);
    chrome.test.assertNoLastError();
    chrome.test.assertEq(
        'Fake DLC file content', new TextDecoder().decode(contents));
    chrome.test.succeed();
  },

  function testGetVariantTtsDlcContentsDlcNotOnDevice() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const ttsVariant = chrome.accessibilityPrivate.TtsVariant.STANDARD;
    const error = 'Error: TTS language pack with locale is not installed: ' +
        'es-us';
    chrome.accessibilityPrivate.getTtsDlcContents(ttsDlc, ttsVariant, () => {
      chrome.test.assertLastError(error);
      chrome.test.succeed();
    });
  },

  async function testGetVariantTtsDlcContentsSuccess() {
    const ttsDlc = chrome.accessibilityPrivate.DlcType.TTS_ES_US;
    const ttsVariant = chrome.accessibilityPrivate.TtsVariant.STANDARD;
    const contents =
        await chrome.accessibilityPrivate.getTtsDlcContents(ttsDlc, ttsVariant);
    chrome.test.assertNoLastError();
    chrome.test.assertEq(
        'Fake DLC file content', new TextDecoder().decode(contents));
    chrome.test.succeed();
  },

  function testSetCursorPosition() {
    chrome.accessibilityPrivate.setCursorPosition({x: 450, y: 350});
    chrome.test.succeed();
  },

  async function testGetDisplayBoundsSimple() {
    const bounds = await chrome.accessibilityPrivate.getDisplayBounds();
    chrome.test.assertEq(
        '[{"height":600,"left":0,"top":0,"width":800}]',
        JSON.stringify(bounds));
    chrome.test.succeed();
  },

  async function testGetDisplayBoundsHighDPI() {
    const bounds = await chrome.accessibilityPrivate.getDisplayBounds();
    chrome.test.assertEq(
        '[{"height":400,"left":0,"top":0,"width":500}]',
        JSON.stringify(bounds));
    chrome.test.succeed();
  },

  async function testGetDisplayBoundsMultipleDisplays() {
    const bounds = await chrome.accessibilityPrivate.getDisplayBounds();
    chrome.test.assertEq(
        '[{"height":300,"left":0,"top":0,"width":400},' +
            '{"height":300,"left":400,"top":0,"width":400}]',
        JSON.stringify(bounds));
    chrome.test.succeed();
  },

  function testInstallFaceGazeAssetsFail() {
    chrome.accessibilityPrivate.installFaceGazeAssets(() => {
      chrome.test.assertLastError(`Couldn't retrieve FaceGaze assets.`);
      chrome.test.succeed();
    });
  },

  async function testInstallFaceGazeAssetsSuccess() {
    const assets = await chrome.accessibilityPrivate.installFaceGazeAssets();
    chrome.test.assertTrue(Boolean(assets));
    chrome.test.assertTrue(Object.keys(assets).length === 2);
    for (const [key, value] of Object.entries(assets)) {
      const fileContents = new TextDecoder().decode(value);
      if (key === 'model') {
        chrome.test.assertEq('Fake facelandmarker model', fileContents);
      } else if (key === 'wasm') {
        chrome.test.assertEq('Fake mediapipe web assembly', fileContents);
      } else {
        chrome.test.fail();
      }
    }

    chrome.test.succeed();
  },

  async function testUpdateFaceGazeBubble() {
    const update = chrome.accessibilityPrivate.updateFaceGazeBubble;
    chrome.test.notifyPass();
    update('Hello world');
    await chrome.test.sendMessage('Confirm hello world');
    update('');
    await chrome.test.sendMessage('Confirm empty text');
    chrome.test.succeed();
  }
];

chrome.test.getConfig((config) => {
  chrome.test.runTests(availableTests.filter((testFunc) => {
    return testFunc.name == config.customArg;
  }));
});
