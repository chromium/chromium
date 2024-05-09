// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {TimezoneSelectorElement} from 'chrome://os-settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('<timezone-selector>', function() {
  let timezoneSelector: TimezoneSelectorElement;

  teardown(function() {
    timezoneSelector.remove();
  });

  test('Per-user timezone disabled', async () => {
    timezoneSelector = document.createElement('timezone-selector');
    timezoneSelector.prefs = {
      'cros': {
        'flags': {
          'per_user_timezone_enabled': {
            value: false,
          },
        },
      },
    };
    document.body.appendChild(timezoneSelector);

    flush();

    assertEquals(
        null,
        timezoneSelector.shadowRoot!.querySelector('#userTimeZoneSelector'));
  });

  test('Per-user timezone enabled', async () => {
    timezoneSelector = document.createElement('timezone-selector');
    timezoneSelector.prefs = {
      'cros': {
        'flags': {
          'per_user_timezone_enabled': {
            value: true,
          },
        },
      },
      generated: {
        resolve_timezone_by_geolocation_on_off: {
          key: 'generated.resolve_timezone_by_geolocation_on_off',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
    };
    document.body.appendChild(timezoneSelector);

    flush();

    assert(timezoneSelector.shadowRoot!.querySelector('#userTimeZoneSelector'));
  });
});
