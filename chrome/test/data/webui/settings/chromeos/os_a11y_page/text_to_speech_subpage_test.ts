// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {SettingsTtsSubpageElement} from 'chrome://os-settings/chromeos/lazy_load.js';
import {Router, routes, TtsSubpageBrowserProxy, TtsSubpageBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestTtsSubpageBrowserProxy extends TestBrowserProxy implements
    TtsSubpageBrowserProxy {
  constructor() {
    super([
      'getAllTtsVoiceData',
      'getTtsExtensions',
      'previewTtsVoice',
      'wakeTtsEngine',
      'refreshTtsVoices',
    ]);
  }

  getAllTtsVoiceData(): void {
    this.methodCalled('getAllTtsVoiceData');
  }

  getTtsExtensions(): void {
    this.methodCalled('getTtsExtensions');
  }

  previewTtsVoice(previewText: string, previewVoice: string): void {
    this.methodCalled('previewTtsVoice', [previewText, previewVoice]);
  }

  wakeTtsEngine(): void {
    this.methodCalled('wakeTtsEngine');
  }

  refreshTtsVoices(): void {
    this.methodCalled('refreshTtsVoices');
  }
}

suite('<settings-tts-subpage>', function() {
  let ttsPage: SettingsTtsSubpageElement;
  let browserProxy: TestTtsSubpageBrowserProxy;

  function getDefaultPrefs() {
    return {
      intl: {
        accept_languages: {
          key: 'intl.accept_languages',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: '',
        },
      },
      settings: {
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
    TtsSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    ttsPage = document.createElement('settings-tts-subpage');
    ttsPage.prefs = getDefaultPrefs();
    document.body.appendChild(ttsPage);
    flush();
  });

  teardown(function() {
    ttsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to text to speech rate', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1503');
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS, params);

    flush();

    const deepLinkElement =
        ttsPage.shadowRoot!.querySelector('#textToSpeechRate')!.shadowRoot!
            .querySelector<HTMLElement>('cr-slider');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech rate slider should be focused for settingId=1503.');
  });

  test('Deep link to text to speech engines', async () => {
    ttsPage.extensions = [{
      name: 'extension1',
      extensionId: 'extension1_id',
      optionsPage: 'extension1_page',
    }];
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '1507');
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS, params);

    const deepLinkElement = ttsPage.shadowRoot!.querySelector<HTMLElement>(
        '#extensionOptionsButton_0');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech engine options should be focused for settingId=1507.');
  });
});
