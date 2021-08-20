// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {convertFrequencyToChannel, convertFrequencyToFiveGhzChannel} from 'chrome://diagnostics/frequency_channel_utils.js';

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

  test('ConvertFrequencyToChannel', () => {
    // Frequencies outside of handled ranges.
    assertEquals(convertFrequencyToChannel(0), null);
    assertEquals(convertFrequencyToChannel(2411), null);
    assertEquals(convertFrequencyToChannel(2496), null);
    assertEquals(convertFrequencyToChannel(5149), null);
    assertEquals(convertFrequencyToChannel(5991), null);
    // Calculates 2.4GHz channels.
    assertEquals(convertFrequencyToChannel(2412), 1);
    assertEquals(convertFrequencyToChannel(2417), 2);
    assertEquals(convertFrequencyToChannel(2422), 3);
    assertEquals(convertFrequencyToChannel(2427), 4);
    assertEquals(convertFrequencyToChannel(2432), 5);
    assertEquals(convertFrequencyToChannel(2437), 6);
    assertEquals(convertFrequencyToChannel(2442), 7);
    assertEquals(convertFrequencyToChannel(2447), 8);
    assertEquals(convertFrequencyToChannel(2452), 9);
    assertEquals(convertFrequencyToChannel(2457), 10);
    assertEquals(convertFrequencyToChannel(2462), 11);
    assertEquals(convertFrequencyToChannel(2467), 12);
    assertEquals(convertFrequencyToChannel(2472), 13);
    // Special 2.4GHz channel range for Japan
    assertEquals(convertFrequencyToChannel(2484), 14);
    assertEquals(convertFrequencyToChannel(2495), 14);
    // 5GHz channels.
    assertEquals(convertFrequencyToChannel(5160), 32);
    assertEquals(convertFrequencyToChannel(5980), 196);
  });
}
