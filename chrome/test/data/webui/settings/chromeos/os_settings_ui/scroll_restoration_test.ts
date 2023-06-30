// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the CrOS Settings UI testing the scroll position
 * restoration after page navigations.
 * Separated into a separate file to mitigate test timeouts.
 */

import {CrSettingsPrefs, OsSettingsUiElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<os-settings-ui> Scroll Restoration', () => {
  let ui: OsSettingsUiElement;

  setup(async () => {
    // Mimic styles from HTML document to enable the scrollable container
    document.documentElement.style.height = '100%';
    document.body.style.height = '100%';

    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    ui.remove();
  });

  test(
      'Top-level page scroll position should be restored after navigating ' +
          'back from a subpage',
      async () => {
        const containerEl =
            ui.shadowRoot!.querySelector<HTMLElement>('#container');
        assert(containerEl);

        // Scroll to bottom of the scrollable container
        const expectedScrollValue =
            containerEl.scrollHeight - containerEl.offsetHeight;
        containerEl.scrollTo({top: expectedScrollValue});
        assertEquals(expectedScrollValue, containerEl.scrollTop);

        // Enter a subpage
        Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS);
        await waitAfterNextRender(containerEl);
        assertEquals(
            0, containerEl.scrollTop,
            'Container scroll position should be at top of subpage.');

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(containerEl);

        assertEquals(
            expectedScrollValue, containerEl.scrollTop,
            'Expected scroll position was not restored.');
      });
});
