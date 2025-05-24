// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for service unit tests. Unlike
 * other tests, these tests are not interacting with the real
 * extension APIs (and not mocking them out via a TestService).
 */

import {Service} from 'chrome://extensions/extensions.js';
import type {ServiceInterface} from 'chrome://extensions/extensions.js';

suite('ExtensionServiceUnitTest', function() {
  let service: ServiceInterface;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    service = Service.getInstance();
  });

  // Tests that calling `setEnabled()` in a manner that will cause the
  // management API to return an error doesn't result in a JS runtime error.
  // Regression test for https://crbug.com/397619884.
  test('Calling setEnabled() does not cause a runtime error', async () => {
    // Call `setItemEnabled` with an arbitrary ID. There's no extension with
    // this ID installed, so the call will fail. This should *not* throw a
    // runtime error, since the underlying `setEnabled()` call can legitimately
    // fail for valid reasons (such as the extension being removed in the time
    // between the click and the call, or the user canceling a re-enable
    // dialog).
    await service.setItemEnabled('a'.repeat(32), true);
  });
});
