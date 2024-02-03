// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://internet-config-dialog/internet_config_dialog.js';

import {InternetConfigDialogElement} from 'chrome://internet-config-dialog/internet_config_dialog.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('internet-config-dialog', () => {
  let internetConfigDialog: InternetConfigDialogElement;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  async function init() {
    internetConfigDialog = document.createElement('internet-config-dialog');
    document.body.appendChild(internetConfigDialog);
    await flushAsync();
  }

  [false, true].forEach(isJellyEnabled => {
    test(
        `CSS theme is updated when isJellyEnabled is ${isJellyEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            isJellyEnabled: isJellyEnabled,
          });
          await init();

          const link = document.head.querySelector(
              `link[href*='chrome://theme/colors.css']`);
          if (isJellyEnabled) {
            assertTrue(!!link);
            assertTrue(document.body.classList.contains('jelly-enabled'));
          } else {
            assertEquals(null, link);
            assertFalse(document.body.classList.contains('jelly-enabled'));
          }
        });
  });
});
