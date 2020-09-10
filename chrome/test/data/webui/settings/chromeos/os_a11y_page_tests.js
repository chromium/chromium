// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('A11yPageTests', function() {
  /** @type {SettingsA11yPageElement} */
  let a11yPage = null;

  setup(function() {
    PolymerTest.clearBody();
    a11yPage = document.createElement('os-settings-a11y-page');
    document.body.appendChild(a11yPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    a11yPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to always show a11y settings', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1500');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_ACCESSIBILITY, params);

    Polymer.dom.flush();

    const deepLinkElement = a11yPage.$$('#optionsInMenuToggle').$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Always show a11y toggle should be focused for settingId=1500.');
  });
});