// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/shared/nearby_progress.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ShareTargetType} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom-webui.js';

import {assertEquals} from '../../chai_assert.js';

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

  /** @return {!ShareTarget} */
  function getDefaultShareTarget() {
    return /** @type {!ShareTarget} */ ({
      id: {high: 0, low: 0},
      name: 'Default Device Name',
      type: ShareTargetType.kPhone,
      imageUrl: {
        url: 'http://google.com/image',
      },
    });
  }

  test('renders component', function() {
    assertEquals('NEARBY-PROGRESS', progressElement.tagName);
  });

  test('renders device name', function() {
    const name = 'Device Name';
    const shareTarget = getDefaultShareTarget();
    shareTarget.name = name;
    progressElement.shareTarget = shareTarget;

    const renderedName =
        progressElement.shadowRoot.querySelector('#device-name').innerText;
    assertEquals(name, renderedName);
  });

  test('renders target image', function() {
    progressElement.shareTarget = getDefaultShareTarget();

    const renderedSource =
        progressElement.shadowRoot.querySelector('#share-target-image').src;
    assertEquals('chrome://image/?http://google.com/image=s68', renderedSource);
  });

  test('renders blank target image', function() {
    const shareTarget = getDefaultShareTarget();
    shareTarget.imageUrl.url = '';
    progressElement.shareTarget = shareTarget;

    const renderedSource =
        progressElement.shadowRoot.querySelector('#share-target-image').src;
    assertEquals('', renderedSource);
  });
});
