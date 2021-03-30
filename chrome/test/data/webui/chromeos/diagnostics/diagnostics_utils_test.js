// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {convertKibToGibDecimalString} from 'chrome://diagnostics/diagnostics_utils.js';
import {assertEquals} from '../../chai_assert.js';

export function diagnosticsUtilsTestSuite() {
  test('ProperlyConvertsKibToGib', () => {
    assertEquals('0', convertKibToGibDecimalString(0, 0));
    assertEquals('0.00', convertKibToGibDecimalString(0, 2));
    assertEquals('0.000000', convertKibToGibDecimalString(0, 6));
    assertEquals('0', convertKibToGibDecimalString(1, 0));
    assertEquals('5.72', convertKibToGibDecimalString(6000000, 2));
    assertEquals('5.722046', convertKibToGibDecimalString(6000000, 6));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 + 1, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 - 1, 2));
    assertEquals('0.999999', convertKibToGibDecimalString(2 ** 20 - 1, 6));
  });
}