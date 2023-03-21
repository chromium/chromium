// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakePointingSticks, fakePointingSticks2, SettingsPerDevicePointingStickElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDevicePointingStick', function() {
  /**
   * @type {?SettingsPerDevicePointingStickElement}
   */
  let perDevicePointingStickPage = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    perDevicePointingStickPage = null;
  });

  function initializePerDevicePointingStickPage(
      pointingSticks = fakePointingSticks) {
    perDevicePointingStickPage =
        document.createElement('settings-per-device-pointing-stick');
    assertTrue(perDevicePointingStickPage != null);
    perDevicePointingStickPage.pointingSticks = pointingSticks;
    document.body.appendChild(perDevicePointingStickPage);
    return flushTasks();
  }

  test('PointingStick page updates with new pointing sticks', async () => {
    await initializePerDevicePointingStickPage();
    let subsections = perDevicePointingStickPage.shadowRoot.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(fakePointingSticks.length, subsections.length);

    // Check the number of subsections when the pointing stick list is updated.
    perDevicePointingStickPage.pointingSticks = fakePointingSticks2;
    await flushTasks();
    subsections = perDevicePointingStickPage.shadowRoot.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(fakePointingSticks2.length, subsections.length);
  });

  test(
      'Display correct name used for internal/external pointing sticks',
      async () => {
        await initializePerDevicePointingStickPage();
        const subsections =
            perDevicePointingStickPage.shadowRoot.querySelectorAll(
                'settings-per-device-pointing-stick-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const name =
              subsections[i].shadowRoot.querySelector('h2').textContent;
          if (fakePointingSticks[i].isExternal) {
            assertEquals(fakePointingSticks[i].name, name);
          } else {
            assertTrue(subsections[i].i18nExists('builtInPointingStickName'));
            assertEquals('Built-in TrackPoint', name);
          }
        }
      });
});