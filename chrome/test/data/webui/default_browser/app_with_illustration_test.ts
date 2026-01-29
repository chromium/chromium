// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://default-browser-modal/app.js';

import type {DefaultBrowserModalAppElement} from 'chrome://default-browser-modal/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('DefaultBrowserModalAppWithIllustrationTest', function() {
  let app: DefaultBrowserModalAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({useSettingsIllustration: true});
    app = document.createElement('default-browser-modal-app');
    document.body.appendChild(app);
  });

  test('CheckLayout', async function() {
    assertTrue(app.useSettingsIllustration);

    const icon = app.shadowRoot.querySelector('#icon');
    assertTrue(isVisible(icon));
    assertEquals('chrome_logo.svg', icon!.getAttribute('src'));

    const headerBackground = app.shadowRoot.querySelector('#header-background');
    assertFalse(isVisible(headerBackground));

    const illustration =
        app.shadowRoot.querySelector<HTMLElement>('#illustration');
    assertTrue(!!illustration);

    const illustrationImg = illustration.querySelector('img');
    assertTrue(!!illustrationImg);

    const illustrationImgLoadPromise = eventToPromise('load', illustrationImg);
    await illustrationImgLoadPromise;

    assertTrue(isVisible(illustration));
    assertTrue(isVisible(illustrationImg));
    assertEquals(
        'settings_illustration.svg', illustrationImg.getAttribute('src'));
  });
});
