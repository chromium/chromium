// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//glic/shared/guest_view/slim_webview.js';
import '//glic/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SlimWebViewTest', function() {
  test('LoadEvents', async function() {
    const webviewUrl = loadTimeData.getString('glicGuestURL');
    assertTrue(webviewUrl.length > 0);

    const webview = document.createElement('webview');
    document.body.appendChild(webview);

    const loadStartPromise = eventToPromise('loadstart', webview);
    const loadCommitPromise = eventToPromise('loadcommit', webview);
    const contentLoadPromise = eventToPromise('contentload', webview);

    webview.src = webviewUrl;

    const [loadStartEvent, loadCommitEvent] = await Promise.all([
      loadStartPromise,
      loadCommitPromise,
      contentLoadPromise,
    ]);

    assertEquals(webviewUrl, loadStartEvent.url);
    assertEquals(webviewUrl, loadCommitEvent.url);
  });
});
