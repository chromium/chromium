// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {convertFrequencyToFiveGhzChannel} from 'chrome://diagnostics/frequency_channel_utils.js';

import {assertEquals} from '../../chai_assert.js';

export function frequencyChannelUtilsTestSuite() {
  test('ConvertFrequencyToFiveGhzChannel', () => {
    // Frequency not in map.
    assertEquals(null, convertFrequencyToFiveGhzChannel(5049));
    assertEquals(null, convertFrequencyToFiveGhzChannel(5351));
    assertEquals(null, convertFrequencyToFiveGhzChannel(5469));
    assertEquals(null, convertFrequencyToFiveGhzChannel(5991));
    // Frequency in map.
    assertEquals(32, convertFrequencyToFiveGhzChannel(5160));
    assertEquals(46, convertFrequencyToFiveGhzChannel(5230));
    assertEquals(196, convertFrequencyToFiveGhzChannel(5980));
  });
}
