// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';

import 'chrome://nearby/nearby_progress.js';
import 'chrome://nearby/nearby_share.mojom-lite.js';

import {assertEquals} from '../chai_assert.js';

suite('ProgressTest', function() {
  /** @type {!NearbyProgressElement} */
  let progressElement;

  setup(function() {
    progressElement = /** @type {!NearbyProgressElement} */ (
        document.createElement('nearby-progress'));
    document.body.appendChild(progressElement);
  });

  teardown(function() {
    progressElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-PROGRESS', progressElement.tagName);
  });

  test('renders device name', function() {
    const name = 'Device Name';
    const shareTarget = /** @type {!nearbyShare.mojom.ShareTarget} */ ({
      id: {high: 0, low: 0},
      name,
      type: nearbyShare.mojom.ShareTargetType.kPhone,
    });
    progressElement.shareTarget = shareTarget;

    const renderedName = progressElement.$$('#device-name').textContent;
    assertEquals(name, renderedName);
  });
});
