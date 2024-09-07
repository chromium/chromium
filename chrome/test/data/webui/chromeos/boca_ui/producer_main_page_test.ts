// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

suite('ClientDelegateTest', function() {
  test('app bundle should have been loaded', async () => {
    assertTrue(isVisible(document.querySelector('boca-app')));
  });

  test('producer home page should have been loaded', async () => {
    assertTrue(isVisible(
        document.querySelector('boca-app')
            ?.shadowRoot?.querySelector('teacher-view') as Element));
  });
});
