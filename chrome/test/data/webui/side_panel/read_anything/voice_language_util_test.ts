// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_language_util.js';

import {mojoVoicePackStatusToVoicePackStatusEnum, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_language_util.js';
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
});
