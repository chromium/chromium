// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundManager} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createBackgroundImage} from './test_support.js';

// Tests the chrome://new-tab-page/custom_background_image?url=... endpoint.

suite('NewTabPageBackgroundImageTest', () => {
  test('IframeLoadsAndSendsBackLoadedMessage', async () => {
    // Setup.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const iframe = document.createElement('iframe');
    iframe.id = 'backgroundImage';
    document.body.appendChild(iframe);
    const backgroundManager = new BackgroundManager();
    const whenLoadMessageSent = eventToPromise('message', window);

    // Act.
    const imgUrl = 'chrome-untrusted://resources/images/add.svg';
    backgroundManager.setBackgroundImage(createBackgroundImage(imgUrl));

    // Assert.
    const message = await whenLoadMessageSent;
    assertEquals('loaded', message.data.messageType);
  });
});
