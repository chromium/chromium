// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SelectToSpeakSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSelectToSpeakSubpageBrowserProxy extends TestBrowserProxy
    implements SelectToSpeakSubpageBrowserProxy {
  constructor() {
    super([
      'getAllTtsVoiceData',
      'getAppLocale',
      'previewTtsVoice',
      'refreshTtsVoices',
    ]);
  }

  getAllTtsVoiceData(): void {
    const voices = [
      {
        displayLanguage: 'English',
        displayLanguageAndCountry: 'English (United States)',
        eventTypes:
            ['start', 'end', 'word', 'interrupted', 'cancelled', 'error'],
        extensionId: 'gjjabgpgjpampikjhjpfhneeoapjbjaf',
        lang: 'en-US',
        voiceName: 'Chrome OS US English',
      },
      {
        displayLanguage: 'Indonesian',
        displayLanguageAndCountry: 'Indonesian (Indonesia)',
        eventTypes:
            ['start', 'end', 'word', 'interrupted', 'cancelled', 'error'],
        extensionId: 'gjjabgpgjpampikjhjpfhneeoapjbjaf',
        lang: 'id-ID',
        voiceName: 'Chrome OS हिन्दी',
      },
      {
        eventTypes:
            ['start', 'end', 'word', 'interrupted', 'cancelled', 'error'],
        extensionId: 'jacnkoglebceckolkoapelihnglgaicd',
        lang: '',
        voiceName: 'default-wavenet',
      },
      {
        displayLanguage: 'Bangla',
        displayLanguageAndCountry: 'Bangla (India)',
        eventTypes:
            ['start', 'end', 'word', 'interrupted', 'cancelled', 'error'],
        extensionId: 'jacnkoglebceckolkoapelihnglgaicd',
        lang: 'bn_in',
        voiceName: 'bnm',
      },
      {
        displayLanguage: 'Bangla',
        displayLanguageAndCountry: 'Bangla (India)',
        eventTypes:
            ['start', 'end', 'word', 'interrupted', 'cancelled', 'error'],
        extensionId: 'jacnkoglebceckolkoapelihnglgaicd',
        lang: 'bn_in',
        voiceName: 'bnx',
      },
      {
        displayLanguage: 'Turkish',
        displayLanguageAndCountry: 'Turkish',
        eventTypes: [
          'start',
          'end',
          'word',
          'sentence',
          'interrupted',
          'cancelled',
          'error',
        ],
        extensionId: 'dakbfdmgjiabojdgbiljlhgjbokobjpg',
        lang: 'tr',
        voiceName: 'eSpeak Turkish',
      },
    ];
    webUIListenerCallback('all-sts-voice-data-updated', voices);
  }

  getAppLocale(): void {
    this.methodCalled('getAppLocale');
  }

  previewTtsVoice(previewText: string, previewVoice: string): void {
    this.methodCalled('previewTtsVoice', [previewText, previewVoice]);
  }

  refreshTtsVoices(): void {
    this.methodCalled('refreshTtsVoices');
  }
}
