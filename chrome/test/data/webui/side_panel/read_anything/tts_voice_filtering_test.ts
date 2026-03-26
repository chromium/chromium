// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {getFilteredVoiceList} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';

suite('tts_utils', () => {
  test('getFilteredVoiceList filters remote voices', () => {
    const voice1 = {
      default: true,
      name: 'Eitan',
      lang: 'en-us',
      localService: false,
      voiceURI: '',
    };
    const voice2 = {
      default: false,
      name: 'Lauren',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };
    let possibleVoices: SpeechSynthesisVoice[] = [voice1];

    // Remote voices not filtered out with just one voice.
    assertDeepEquals([voice1], getFilteredVoiceList(possibleVoices));

    possibleVoices = [voice1, voice2];
    // Remote voices filtered out when a local voice exists
    assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
  });

  test('getFilteredVoiceList filters Android voices', () => {
    const voice1 = {
      default: false,
      name: 'Xiangu',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };
    const voice2 = {
      default: true,
      name: 'Kristi (Android)',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };

    // Android voices should be filtered out only on ChromeOS Ash
    const possibleVoices: SpeechSynthesisVoice[] = [voice1, voice2];

    if (chrome.readingMode.isChromeOsAsh) {
      assertDeepEquals([voice1], getFilteredVoiceList(possibleVoices));
    } else {
      assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
    }
  });

  test('getFilteredVoiceList filters eSpeak voices', () => {
    const voice1 = createSpeechSynthesisVoice(
        {default: true, name: 'eSpeak Yu', localService: true, lang: 'en-us'});
    const voice2 = createSpeechSynthesisVoice(
        {default: true, name: 'eSpeak Kristi', localService: true, lang: 'cy'});
    const voice3 = createSpeechSynthesisVoice({
      default: true,
      name: 'eSpeak Lauren',
      localService: true,
      lang: 'en-cb',
    });

    const possibleVoices: SpeechSynthesisVoice[] = [voice1, voice2, voice3];

    if (chrome.readingMode.isChromeOsAsh) {
      // Welsh isn't a Google TTS locale, so the Welsh eSpeak voice should be
      // kept, but the English eSpeak voices should be filtered out because
      // Google TTS voices in English (even if in a different locale) exist.
      assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
    } else {
      // en-us is kept because it is an exact Google TTS locale. cy is kept
      // because there is no Google TTS locale that supports it. en-cb is not
      // kept because there are other locales supported by Google TTS for the
      // same language.
      assertDeepEquals([voice1, voice2], getFilteredVoiceList(possibleVoices));
    }
  });

  test(
      'getFilteredVoiceList returns only Google voices and one system voice ' +
          'per language',
      () => {
        const voice1 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Eitan',
          localService: true,
          lang: 'en-us',
        });
        const voice2 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Shari',
          localService: true,
          lang: 'en-us',
        });
        const voice3 = createSpeechSynthesisVoice({
          default: false,
          name: 'Lauren',
          localService: true,
          lang: 'en-us',
        });
        const voice4 = createSpeechSynthesisVoice({
          default: true,
          name: 'Kristi',
          localService: true,
          lang: 'en-us',
        });
        const voice5 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Cat',
          localService: true,
          lang: 'pt-br',
        });
        const voice6 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Dog',
          localService: true,
          lang: 'pt-br',
        });
        const voice7 = createSpeechSynthesisVoice({
          default: false,
          name: 'Mouse',
          localService: true,
          lang: 'pt-br',
        });
        const voice8 = createSpeechSynthesisVoice({
          default: true,
          name: 'Bird',
          localService: true,
          lang: 'pt-br',
        });

        const possibleVoices: SpeechSynthesisVoice[] =
            [voice1, voice2, voice3, voice4, voice5, voice6, voice7, voice8];

        if (chrome.readingMode.isChromeOsAsh) {
          // Don't filter out any system voices on ChromeOS.
          assertDeepEquals(
              possibleVoices, getFilteredVoiceList(possibleVoices));
        } else {
          // Keep only the default system voice per language.
          assertDeepEquals(
              [voice1, voice2, voice4, voice5, voice6, voice8],
              getFilteredVoiceList(possibleVoices));
        }
      });
});
