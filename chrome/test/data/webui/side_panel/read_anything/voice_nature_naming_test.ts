// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {AVAILABLE_GOOGLE_TTS_LOCALES, getVoiceNatureNaming, VOICE_NATURE_NAMING_BY_LOCALE} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('VoiceNatureNaming', () => {
  test('no leading or trailing whitespace in any field', () => {
    for (const [locale, entries] of Object.entries(
             VOICE_NATURE_NAMING_BY_LOCALE)) {
      assertEquals(locale.trim(), locale, `locale "${locale}"`);
      for (const [engineVoiceName, naming] of Object.entries(entries)) {
        assertEquals(
            engineVoiceName.trim(), engineVoiceName,
            `voice key "${engineVoiceName}"`);
        assertEquals(
            naming.natureName.trim(), naming.natureName,
            `natureName of "${engineVoiceName}"`);
        assertEquals(
            naming.description.trim(), naming.description,
            `description of "${engineVoiceName}"`);
      }
    }
  });

  test('voice keys are unique across locales', () => {
    // Duplicate keys *within* one locale are a compile error, since they are
    // duplicate properties of one object literal. Only cross-locale duplicates
    // can reach runtime, and they are invisible in the flattened table because
    // new Map() silently keeps the last of a repeated key. So the count is
    // compared against the authoring form rather than against the table.
    const authoredEntries = Object.values(VOICE_NATURE_NAMING_BY_LOCALE)
                                .flatMap(entries => Object.entries(entries));

    assertGT(authoredEntries.length, 0);
    assertEquals(authoredEntries.length, new Map(authoredEntries).size);
  });

  test('nature names are unique within a locale', () => {
    // Nature names are values, not keys, so unlike duplicate voice keys these
    // are not caught by the compiler. Descriptions are deliberately excluded:
    // repeated descriptions within a locale are intentional.
    for (const [locale, entries] of Object.entries(
             VOICE_NATURE_NAMING_BY_LOCALE)) {
      const seen = new Set<string>();
      for (const [engineVoiceName, naming] of Object.entries(entries)) {
        assertFalse(
            seen.has(naming.natureName),
            `duplicate natureName "${naming.natureName}" in locale ` +
                `"${locale}", second occurrence on "${engineVoiceName}"`);
        seen.add(naming.natureName);
      }
    }
  });

  test('required fields are non-empty', () => {
    for (const entries of Object.values(VOICE_NATURE_NAMING_BY_LOCALE)) {
      for (const [engineVoiceName, naming] of Object.entries(entries)) {
        assertGT(
            naming.natureName.length, 0, `natureName of "${engineVoiceName}"`);
        assertGT(
            naming.description.length, 0,
            `description of "${engineVoiceName}"`);
      }
    }
  });

  test('locale set matches AVAILABLE_GOOGLE_TTS_LOCALES exactly', () => {
    const authoredLocales = Object.keys(VOICE_NATURE_NAMING_BY_LOCALE).sort();
    const googleTtsLocales = [...AVAILABLE_GOOGLE_TTS_LOCALES].sort();

    assertDeepEquals(googleTtsLocales, authoredLocales);
  });

  // Expected number of voices per locale. Unlike the other checks in this
  // suite this is a deliberate tripwire rather than an invariant: a failure
  // means the table gained or lost a mapping. Confirm the change against the
  // source data before updating a number here.
  const EXPECTED_VOICE_COUNT_BY_LOCALE: Record<string, number> = {
    'bn-bd': 1,
    'cs-cz': 1,
    'da-dk': 4,
    'de-de': 4,
    'el-gr': 1,
    'en-au': 4,
    'en-gb': 6,
    'en-us': 7,
    'es-es': 5,
    'es-us': 4,
    'fi-fi': 1,
    'fil-ph': 4,
    'fr-fr': 5,
    'hi-in': 4,
    'hu-hu': 1,
    'id-id': 4,
    'it-it': 4,
    'ja-jp': 3,
    'km-kh': 1,
    'ko-kr': 4,
    'nb-no': 5,
    'ne-np': 1,
    'nl-nl': 5,
    'pl-pl': 5,
    'pt-br': 3,
    'pt-pt': 4,
    'si-lk': 1,
    'sk-sk': 1,
    'sv-se': 5,
    'th-th': 2,
    'tr-tr': 5,
    'uk-ua': 1,
    'vi-vn': 5,
    'yue-hk': 5,
  };

  const LOCALES_EXEMPT_FROM_NUMBERING = new Set<string>(['th-th']);

  test('each locale has the expected number of voices', () => {
    const actual: Record<string, number> = {};
    for (const [locale, entries] of Object.entries(
             VOICE_NATURE_NAMING_BY_LOCALE)) {
      actual[locale] = Object.keys(entries).length;
    }

    assertDeepEquals(EXPECTED_VOICE_COUNT_BY_LOCALE, actual);
  });

  test('multi-voice locales are numbered contiguously from 1', () => {
    for (const [locale, entries] of Object.entries(
             VOICE_NATURE_NAMING_BY_LOCALE)) {
      const keys = Object.keys(entries);
      if (keys.length < 2 || LOCALES_EXEMPT_FROM_NUMBERING.has(locale)) {
        continue;
      }

      const prefixes = new Set<string>();
      const numbers: number[] = [];
      for (const key of keys) {
        const match = /^(.*) (\d+)(?: \(Natural\))?$/.exec(key);
        assertTrue(
            match !== null, `voice key "${key}" in "${locale}" is unnumbered`);
        prefixes.add(match[1]!);
        numbers.push(Number(match[2]));
      }

      assertEquals(
          1, prefixes.size,
          `voice keys in "${locale}" use more than one prefix: ` +
              `${[...prefixes].join(', ')}`);
      assertDeepEquals(
          Array.from({length: numbers.length}, (_, i) => i + 1),
          numbers.sort((a, b) => a - b),
          `voice numbers in "${locale}" are not 1..${numbers.length}`);
    }
  });

  const ENGLISH_DESCRIPTION = /^[A-Z][a-z]+ tone, (?:low|mid|high) pitch$/;
  const SPANISH_DESCRIPTION =
      /^Voz [a-záéíóúñ]+, tono (?:bajo|medio|alto)$/;

  const DESCRIPTION_PATTERN_BY_LOCALE: Record<string, RegExp> = {
    'en-au': ENGLISH_DESCRIPTION,
    'en-gb': ENGLISH_DESCRIPTION,
    'en-us': ENGLISH_DESCRIPTION,
    'es-es': SPANISH_DESCRIPTION,
    'es-us': SPANISH_DESCRIPTION,
  };

  test('English and Spanish descriptors follow their grammar', () => {
    for (const [locale, pattern] of Object.entries(
             DESCRIPTION_PATTERN_BY_LOCALE)) {
      const entries = VOICE_NATURE_NAMING_BY_LOCALE[locale];
      assertTrue(entries !== undefined, `locale "${locale}" is missing`);

      for (const [engineVoiceName, naming] of Object.entries(entries)) {
        assertTrue(
            pattern.test(naming.description),
            `description "${naming.description}" of "${engineVoiceName}" ` +
                `does not match the "${locale}" grammar`);
      }
    }
  });

  test('lookup returns the exact structured value for a known key', () => {
    assertDeepEquals(
        {natureName: 'Pebble', description: 'Gentle tone, mid pitch'},
        getVoiceNatureNaming('Google US English 1 (Natural)'));
  });

  test('lookup returns null for an unknown key', () => {
    assertEquals(null, getVoiceNatureNaming('Chrome OS US English 1'));
  });

  test('lookup does not normalise: padded and case-variant keys miss', () => {
    assertTrue(getVoiceNatureNaming('Google US English 1 (Natural)') !== null);

    assertEquals(null, getVoiceNatureNaming('Google US English 1 (Natural) '));
    assertEquals(null, getVoiceNatureNaming(' Google US English 1 (Natural)'));
    assertEquals(null, getVoiceNatureNaming('google us english 1 (natural)'));
  });
});
