// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {TtsSubpageBrowserProxy}
 */
class TestTtsSubpageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAllTtsVoiceData',
      'getTtsExtensions',
      'previewTtsVoice',
      'wakeTtsEngine',
    ]);
  }

  /** @override */
  getAllTtsVoiceData() {
    this.methodCalled('getAllTtsVoiceData');
  }

  /** @override */
  getTtsExtensions() {
    this.methodCalled('getTtsExtensions');
  }

  /** @override */
  previewTtsVoice(previewText, previewVoice) {
    this.methodCalled('previewTtsVoice', [previewText, previewVoice]);
  }

  /** @override */
  wakeTtsEngine() {
    this.methodCalled('wakeTtsEngine');
  }
}

suite('TextToSpeechSubpageTests', function() {
  /** @type {SettingsTtsSubpageElement} */
  let ttsPage = null;

  /** @type {?TestTtsSubpageBrowserProxy} */
  let browserProxy = null;

  function getDefaultPrefs() {
    return {
      settings: {
        language: {
          preferred_languages: {
            key: 'settings.language.preferred_languages',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: '',
          },
        },
        tts: {
          lang_to_voice_name: {
            key: 'prefs.settings.tts.lang_to_voice_name',
            type: chrome.settingsPrivate.PrefType.DICTIONARY,
            value: {},
          },
        },
      },
    };
  }

  setup(function() {
    browserProxy = new TestTtsSubpageBrowserProxy();
    TtsSubpageBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    ttsPage = document.createElement('settings-tts-subpage');
    ttsPage.prefs = getDefaultPrefs();
    document.body.appendChild(ttsPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    ttsPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to text to speech rate', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1503');
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_TTS_SETTINGS, params);

    Polymer.dom.flush();

    const deepLinkElement = ttsPage.$$('#textToSpeechRate').$$('cr-slider');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech rate slider should be focused for settingId=1503.');
  });

  test('Deep link to text to speech engines', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    ttsPage.extensions = [{
      name: 'extension1',
      extensionId: 'extension1_id',
      optionsPage: 'extension1_page'
    }];
    Polymer.dom.flush();

    const params = new URLSearchParams;
    params.append('settingId', '1507');
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_TTS_SETTINGS, params);

    const deepLinkElement = ttsPage.$$('#extensionOptionsButton_0');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech engine options should be focused for settingId=1507.');
  });
});