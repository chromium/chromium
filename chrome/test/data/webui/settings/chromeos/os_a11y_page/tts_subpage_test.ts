// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {SettingsTtsSubpageElement} from 'chrome://os-settings/chromeos/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('TtsSubpage', function() {
  let ttsSubpage: SettingsTtsSubpageElement;

  setup(function() {
    ttsSubpage = document.createElement('settings-tts-subpage');
    document.body.appendChild(ttsSubpage);
    flush();
  });

  test('Preview Voice Select Options', function() {
    ttsSubpage.prefs = {
      intl: {
        accept_languages: {
          value: '',
        },
      },
      settings: {
        'tts': {
          'lang_to_voice_name': {
            value: '',
          },
        },
      },
    };

    ttsSubpage.set('allVoices', [
      {id: 'A', displayLanguage: 'Klingon', name: 'Star Trek'},
      {id: 'B', displayLanguage: 'Goa\'uld', name: 'Star Gate'},
      {id: 'C', displayLanguage: 'Dothraki', name: 'Game of Thrones'},
    ]);
    flush();

    const previewVoice = ttsSubpage.$.previewVoice;
    assertTrue(!!previewVoice);
    assertEquals(3, previewVoice.length);

    // Check one of the language option details.
    const secondVoice = ttsSubpage.shadowRoot!.querySelector('option[value=B]');
    assertTrue(!!secondVoice);
    assertEquals(
        'Goa\'uld - Star Gate', String(secondVoice.textContent).trim());
  });
});
