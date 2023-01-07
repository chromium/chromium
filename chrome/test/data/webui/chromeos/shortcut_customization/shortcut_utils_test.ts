// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isCustomizationDisabled} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('shortcutUtilsTest', function() {
  test('CustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    assertTrue(isCustomizationDisabled());
  });

  test('CustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    assertFalse(isCustomizationDisabled());
  });
});