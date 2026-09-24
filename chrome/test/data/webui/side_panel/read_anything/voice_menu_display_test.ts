// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {VoiceDropdownItem} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {computeDownloadingMessages, computeErrorMessages, computeVoiceDropdown, getVoiceDisplayName, getVoiceNatureNaming, getVoiceTitle, getVoiceTitleAndNatureNaming, isVoicePreviewSpinning, NotificationType, stringToHtmlTestId, voiceQualityRankComparator} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, setupTestEnvironment} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';

suite('VoiceMenuDisplay', () => {
  let audioBrowserProxy: TestAudioBrowserProxy;

  const googleNaturalVoice = createSpeechSynthesisVoice(
      {name: 'Google US English (Natural)', lang: 'en-US'});
  const googleStandardVoice =
      createSpeechSynthesisVoice({name: 'Google US English', lang: 'en-US'});
  const italianVoice =
      createSpeechSynthesisVoice({name: 'Google Italian', lang: 'it-IT'});
  // Key for getVoiceNatureNaming. googleNaturalVoice can't be used as key
  // due to the missing voice number.
  const mappedVoice = createSpeechSynthesisVoice(
      {name: 'Google US English 1 (Natural)', lang: 'en-US'});
  const mappedSpanishVoice = createSpeechSynthesisVoice(
      {name: 'Google español de Estados Unidos 1 (Natural)', lang: 'es-US'});

  setup(() => {
    const result = setupTestEnvironment();
    audioBrowserProxy = result.audioBrowserProxy;

    loadTimeData.overrideValues({
      voiceSelectionLabel: 'Voice',
      systemVoiceLabel: 'System default voice',
      readingModeVoiceMenuNoSpace: 'Out of storage for $1',
      readingModeVoiceMenuNoInternet: 'No internet connection for $1',
      readingModeVoiceMenuDownloading: 'Downloading $1...',
    });

    audioBrowserProxy.localeToDisplayName = {
      'en-us': 'English (United States)',
      'it': 'Italian',
      'it-it': 'Italian',
      'es-es': 'Spanish',
    };
  });

  test('getVoiceTitle returns name for Google voices', () => {
    assertEquals('Google US English', getVoiceTitle(googleStandardVoice));
    assertEquals(
        'Google US English (Natural)', getVoiceTitle(googleNaturalVoice));
  });

  test('getVoiceTitleAndNatureNaming resolves both display fields', () => {
    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const mapped = getVoiceTitleAndNatureNaming(voice);

      assertEquals(voice.name, mapped.title);
      assertDeepEquals(getVoiceNatureNaming(voice.name), mapped.natureNaming);
    }

    const unmapped = getVoiceTitleAndNatureNaming(googleNaturalVoice);

    assertEquals(googleNaturalVoice.name, unmapped.title);
    assertEquals(null, unmapped.natureNaming);
  });

  test('getVoiceTitleAndNatureNaming handles no selected voice', () => {
    // The point here is that the naming lookup is skipped rather than called
    // with a placeholder string.
    const none = getVoiceTitleAndNatureNaming(null);

    assertEquals('Voice', none.title);
    assertEquals(null, none.natureNaming);
  });

  test('getVoiceDisplayName prefers the nature name', () => {
    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const mapped = getVoiceTitleAndNatureNaming(voice);

      assertEquals(
          getVoiceNatureNaming(voice.name)!.natureName,
          getVoiceDisplayName(mapped));
    }
  });

  test('getVoiceDisplayName falls back to the title', () => {
    const unmapped = getVoiceTitleAndNatureNaming(googleNaturalVoice);

    assertEquals(googleNaturalVoice.name, getVoiceDisplayName(unmapped));
  });

  test('stringToHtmlTestId removes spaces and parentheses', () => {
    assertEquals(
        'Google-US-English-Natural',
        stringToHtmlTestId('Google US English (Natural)'));
    assertEquals('David', stringToHtmlTestId('David'));
    assertEquals('Voice-Name', stringToHtmlTestId('Voice Name'));
    assertEquals('', stringToHtmlTestId(''));
  });

  test(
      'voiceQualityRankComparator uses Natural voices ahead of Standard voices',
      () => {
        const naturalVsStandard =
            voiceQualityRankComparator({voice: googleNaturalVoice}, {
              voice: googleStandardVoice,
            });

        assertEquals(-1, naturalVsStandard);

        const standardVsNatural =
            voiceQualityRankComparator({voice: googleStandardVoice}, {
              voice: googleNaturalVoice,
            });

        assertEquals(1, standardVsNatural);
      });

  test(
      'voiceQualityRankComparator treats identical quality tiers as equal',
      () => {
        const compareNatural =
            voiceQualityRankComparator({voice: googleNaturalVoice}, {
              voice: googleNaturalVoice,
            });

        assertEquals(0, compareNatural);

        const compareStandard =
            voiceQualityRankComparator({voice: googleStandardVoice}, {
              voice: googleStandardVoice,
            });

        assertEquals(0, compareStandard);
      });

  test(
      'isVoicePreviewSpinning returns true only before preview is playing',
      () => {
        const spinningItem: VoiceDropdownItem = {
          title: 'Voice',
          natureNaming: null,
          voice: googleNaturalVoice,
          id: 'voice-id',
          selected: false,
          previewInitiated: true,
          previewActuallyPlaying: false,
        };

        assertTrue(isVoicePreviewSpinning(spinningItem));

        const playingItem: VoiceDropdownItem = {
          ...spinningItem,
          previewActuallyPlaying: true,
        };

        assertFalse(isVoicePreviewSpinning(playingItem));

        const idleItem: VoiceDropdownItem = {
          ...spinningItem,
          previewInitiated: false,
          previewActuallyPlaying: false,
        };

        assertFalse(isVoicePreviewSpinning(idleItem));

        const playingNotInitiatedItem: VoiceDropdownItem = {
          ...spinningItem,
          previewInitiated: false,
          previewActuallyPlaying: true,
        };

        assertFalse(isVoicePreviewSpinning(playingNotInitiatedItem));
      });

  test('computeVoiceDropdown filters voices by enabled languages', () => {
    const result = computeVoiceDropdown({
      availableVoices: [googleNaturalVoice, italianVoice],
      enabledLangs: ['EN-US'],
    });

    assertEquals(1, result.groups.length);
    assertEquals(1, result.groups[0]!.voices.length);
    assertEquals(
        googleNaturalVoice.name, result.groups[0]!.voices[0]!.voice.name);
  });

  test(
      'computeVoiceDropdown ignores voices with missing lang attribute', () => {
        const voiceWithNoLang =
            createSpeechSynthesisVoice({name: 'Broken', lang: ''});
        const result = computeVoiceDropdown({
          availableVoices: [googleNaturalVoice, voiceWithNoLang],
          enabledLangs: ['en-us'],
        });

        assertEquals(1, result.groups.length);
        assertEquals(1, result.groups[0]!.voices.length);
        assertEquals(
            googleNaturalVoice.name, result.groups[0]!.voices[0]!.voice.name);
      });

  test(
      'computeVoiceDropdown groups voices and sorts Natural voices first',
      () => {
        const result = computeVoiceDropdown({
          availableVoices: [
            googleStandardVoice,
            italianVoice,
            googleNaturalVoice,
          ],
          enabledLangs: ['en-us', 'it-it'],
          localeToDisplayName: {
            'en-us': 'English (United States)',
            'it-it': 'Italian',
          },
        });

        assertEquals(2, result.groups.length);
        const englishGroup =
            result.groups.find(g => g.language === 'English (United States)')!;
        assertTrue(!!englishGroup);
        assertEquals(2, englishGroup.voices.length);
        // Natural voice must be sorted first
        assertEquals(
            googleNaturalVoice.name, englishGroup.voices[0]!.voice.name);
        assertEquals('Google-US-English-Natural', englishGroup.voices[0]!.id);
        assertEquals(
            googleStandardVoice.name, englishGroup.voices[1]!.voice.name);
        assertEquals('Google-US-English', englishGroup.voices[1]!.id);

        const italianGroup = result.groups.find(g => g.language === 'Italian')!;
        assertTrue(!!italianGroup);
        assertEquals(1, italianGroup.voices.length);
        assertEquals(italianVoice.name, italianGroup.voices[0]!.voice.name);
      });

  test(
      'computeVoiceDropdown flags selected voice and sets hasSelectedVoice' +
          ' to true',
      () => {
        const result = computeVoiceDropdown({
          availableVoices: [googleNaturalVoice, googleStandardVoice],
          enabledLangs: ['en-us'],
          selectedVoice: googleStandardVoice,
        });

        assertTrue(result.hasSelectedVoice);
        assertFalse(result.groups[0]!.voices[0]!.selected);
        assertTrue(result.groups[0]!.voices[1]!.selected);
      });

  test(
      'computeVoiceDropdown sets hasSelectedVoice false when selected voice' +
          ' is not visible',
      () => {
        const missingVoice =
            createSpeechSynthesisVoice({name: 'Missing', lang: 'fr-FR'});
        const result = computeVoiceDropdown({
          availableVoices: [googleNaturalVoice],
          enabledLangs: ['en-us'],
          selectedVoice: missingVoice,
        });

        assertFalse(result.hasSelectedVoice);
        assertFalse(result.groups[0]!.voices[0]!.selected);
      });

  test('computeVoiceDropdown maps a voice with a nature naming entry', () => {
    for (const voice of [mappedVoice, mappedSpanishVoice]) {
      const result = computeVoiceDropdown({
        availableVoices: [voice],
        enabledLangs: [voice.lang.toLowerCase()],
      });

      const item = result.groups[0]!.voices[0]!;

      assertDeepEquals(getVoiceNatureNaming(voice.name), item.natureNaming);
      assertEquals(voice.name, item.title);
    }
  });

  test(
      'computeVoiceDropdown leaves natureNaming null for an unmapped voice',
      () => {
        const result = computeVoiceDropdown({
          availableVoices: [googleNaturalVoice],
          enabledLangs: ['en-us'],
        });

        const item = result.groups[0]!.voices[0]!;

        assertEquals(null, item.natureNaming);
        assertEquals(googleNaturalVoice.name, item.title);
      });

  // "computeVoiceDropdown lets the system voice collapse outrank a mapping" and
  // "computeVoiceDropdown maps a voice with no Google identifier" are
  // complementary tests based on platform (ChromeOS vs other desktops).
  // <if expr="not is_chromeos">
  test(
      'computeVoiceDropdown lets the system voice collapse outrank a mapping',
      () => {
        const mappedSystemVoice = createSpeechSynthesisVoice(
            {name: 'Chrome OS 粵語 1', lang: 'yue-HK'});

        assertTrue(getVoiceNatureNaming(mappedSystemVoice.name) !== null);

        const result = computeVoiceDropdown({
          availableVoices: [mappedSystemVoice],
          enabledLangs: ['yue-hk'],
        });

        // The name is a table key, but getVoiceTitle hid it behind the generic
        // system-voice label.
        const item = result.groups[0]!.voices[0]!;

        assertEquals('System default voice', item.title);
        assertEquals(null, item.natureNaming);
      });
  // </if>

  // <if expr="is_chromeos">
  test('computeVoiceDropdown maps a voice with no Google identifier', () => {
    const mappedSystemVoice = createSpeechSynthesisVoice(
        {name: 'Chrome OS 粵語 1', lang: 'yue-HK'});

    const result = computeVoiceDropdown({
      availableVoices: [mappedSystemVoice],
      enabledLangs: ['yue-hk'],
    });

    const item = result.groups[0]!.voices[0]!;

    assertEquals(mappedSystemVoice.name, item.title);
    assertDeepEquals(
        getVoiceNatureNaming(mappedSystemVoice.name), item.natureNaming);
  });
  // </if>

  test(
      'computeErrorMessages returns empty array when notifications is' +
          ' undefined or empty',
      () => {
        const undefinedErrors =
            computeErrorMessages(undefined, audioBrowserProxy);

        assertEquals(0, undefinedErrors.length);

        const emptyErrors = computeErrorMessages({}, audioBrowserProxy);

        assertEquals(0, emptyErrors.length);
      });

  test('computeErrorMessages formats NO_SPACE and NO_INTERNET messages', () => {
    const notifications = {
      'en-us': NotificationType.NO_SPACE,
      'it-it': NotificationType.NO_INTERNET,
      'es-es': NotificationType.DOWNLOADING,
    };

    const errors = computeErrorMessages(notifications, audioBrowserProxy);

    assertEquals(2, errors.length);
    assertEquals('Out of storage for English (United States)', errors[0]);
    assertEquals('No internet connection for Italian', errors[1]);
    assertEquals(2, audioBrowserProxy.getCallCount('getDisplayNameForLocale'));
  });

  test(
      'computeErrorMessages skips notifications for unresolvable locales',
      () => {
        audioBrowserProxy.reset();
        const notifications = {
          'invalid-lang': NotificationType.NO_SPACE,
        };

        const errors = computeErrorMessages(notifications, audioBrowserProxy);

    assertEquals(0, errors.length);
    assertEquals(
            0, audioBrowserProxy.getCallCount('getDisplayNameForLocale'));
  });

  test(
      'computeDownloadingMessages returns empty array when notifications is' +
          ' undefined or empty',
      () => {
        const undefinedDownloading =
            computeDownloadingMessages(undefined, audioBrowserProxy);

        assertEquals(0, undefinedDownloading.length);

        const emptyDownloading =
            computeDownloadingMessages({}, audioBrowserProxy);

        assertEquals(0, emptyDownloading.length);
      });

  test('computeDownloadingMessages formats DOWNLOADING messages only', () => {
    const notifications = {
      'en-us': NotificationType.NO_SPACE,
      'es-es': NotificationType.DOWNLOADING,
    };

    const downloading =
        computeDownloadingMessages(notifications, audioBrowserProxy);

    assertEquals(1, downloading.length);
    assertEquals('Downloading Spanish...', downloading[0]);
    assertEquals(1, audioBrowserProxy.getCallCount('getDisplayNameForLocale'));
  });
});
