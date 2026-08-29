// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {getDisplayName, getNormalizedDisplayName, isLanguageSearchMatch, isSubstring, sortLanguagesByDisplayName, stripDiacritics} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('LanguageDisplay', () => {
  const localeToDisplayName = {
    'en-us': 'English (United States)',
    'en-gb': 'English (United Kingdom)',
    'es-es': 'Español (España)',
    'pt-br': 'Português (Brasil)',
  };

  test('stripDiacritics removes combining accent marks', () => {
    assertEquals('Espanol (Espana)', stripDiacritics('Español (España)'));
    assertEquals('Portugues (Brasil)', stripDiacritics('Português (Brasil)'));
    assertEquals('English', stripDiacritics('English'));
  });

  test('isSubstring matches case-insensitively', () => {
    assertTrue(isSubstring('Hello World', 'world'));
    assertTrue(isSubstring('Hello World', 'HELLO'));
    assertFalse(isSubstring('Hello World', 'xyz'));
  });

  test(
      'getDisplayName returns mapped name or falls back to lowercase lang',
      () => {
        assertEquals(
            'English (United States)',
            getDisplayName('en-US', localeToDisplayName));
        assertEquals('fr-fr', getDisplayName('FR-FR', localeToDisplayName));
      });

  test('getNormalizedDisplayName returns diacritic-stripped name', () => {
    assertEquals(
        'Espanol (Espana)',
        getNormalizedDisplayName('es-es', localeToDisplayName));
  });

  test(
      'isLanguageSearchMatch handles name, code, and diacritics bidirectionally',
      () => {
        // Display name search
        assertTrue(
            isLanguageSearchMatch('en-us', 'united', localeToDisplayName));
        // Language code search
        assertTrue(isLanguageSearchMatch('en-us', 'en-u', localeToDisplayName));
        // Plain query matching diacritic name
        assertTrue(
            isLanguageSearchMatch('es-es', 'espanol', localeToDisplayName));
        // Diacritic query matching diacritic name
        assertTrue(
            isLanguageSearchMatch('es-es', 'español', localeToDisplayName));
        // No match
        assertFalse(
            isLanguageSearchMatch('en-us', 'french', localeToDisplayName));
      });

  test(
      'sortLanguagesByDisplayName sorts alphabetically by display name', () => {
        const langs = ['es-es', 'en-us', 'en-gb'];
        sortLanguagesByDisplayName(langs, localeToDisplayName);
        // English (United Kingdom), English (United States), Español (España)
        assertEquals('en-gb', langs[0]);
        assertEquals('en-us', langs[1]);
        assertEquals('es-es', langs[2]);
      });
});
