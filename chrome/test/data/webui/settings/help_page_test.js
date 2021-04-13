// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings help page. */

// clang-format off
import 'chrome://settings/settings.js';
import {getPage, getSection} from 'chrome://test/settings/settings_page_test_util.js';
import {waitBeforeNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

// Register mocha tests.
suite('SettingsHelpPage', function() {
  setup(function() {
    PolymerTest.clearBody();
    // The ChromeContentBrowserClient will rewrite chrome://help to
    // chrome://settings/help.
    window.history.pushState('', 'Test', 'chrome://settings/help');
    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);

    // Wait for the dom-if.
    return waitBeforeNextRender(settingsUi);
  });

  test('about section', async () => {
    const page = await getPage('about');
    expectTrue(!!getSection(page, 'about'));
  });
});
