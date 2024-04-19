// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_language_util.js';

import {convertLangOrLocaleForVoicePackManager, mojoVoicePackStatusToVoicePackStatusEnum, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_language_util.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('voice and language utils', () => {
  test('mojoVoicePackStatusToVoicePackStatusEnum', () => {
    // Success codes
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled')),
        VoicePackStatus.NOT_INSTALLED);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kInstalled')),
        VoicePackStatus.INSTALLED);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kInstalling')),
        VoicePackStatus.INSTALLING);

    // Error codes
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kUnknown')),
        VoicePackStatus.ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kOther')),
        VoicePackStatus.ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kWrongId')),
        VoicePackStatus.ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot')),
        VoicePackStatus.ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kAllocation')),
        VoicePackStatus.ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kUnsupportedPlatform')),
        VoicePackStatus.ERROR);
  });

  test('convertLangOrLocaleForVoicePackManager', () => {
    // Converts to locale when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('en'), 'en-us');
    assertEquals(convertLangOrLocaleForVoicePackManager('es'), 'es-es');
    assertEquals(convertLangOrLocaleForVoicePackManager('pt'), 'pt-br');

    // Converts to base language when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('fr-FR'), 'fr');
    assertEquals(convertLangOrLocaleForVoicePackManager('fr-Bf'), 'fr');


    // Converts from unsupported locale to supported locale when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('en-UK'), 'en-us');
    assertEquals(convertLangOrLocaleForVoicePackManager('es-MX'), 'es-es');

    // Keeps base language when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('nl'), 'nl');
    assertEquals(convertLangOrLocaleForVoicePackManager('yue'), 'yue');

    // Keeps locale when necesseary
    assertEquals(convertLangOrLocaleForVoicePackManager('en-GB'), 'en-gb');
    assertEquals(convertLangOrLocaleForVoicePackManager('es-es'), 'es-es');
    assertEquals(convertLangOrLocaleForVoicePackManager('pt-Br'), 'pt-br');

    // Unsupported languages are undefined
    assertEquals(convertLangOrLocaleForVoicePackManager('cn'), undefined);
  });
});
