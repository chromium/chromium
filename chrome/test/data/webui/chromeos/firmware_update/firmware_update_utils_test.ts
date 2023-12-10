// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/firmware_update_utils.js';

import {isAppV2Enabled} from 'chrome://accessory-update/firmware_update_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('FirmwareUpdateUtilsTest', () => {
  test('IsAppV2Enabled', () => {
    loadTimeData.resetForTesting({});
    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: true});
    assertTrue(isAppV2Enabled());

    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: false});
    assertFalse(isAppV2Enabled());
  });
});
