// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AvatarCamera} from 'chrome://personalization/trusted/user/avatar_camera_element.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function AvatarCameraTest() {
  let avatarCameraElement: AvatarCamera;

  setup(function() {
    avatarCameraElement = initElement(AvatarCamera, {open: false});
  });

  teardown(async () => {
    await teardownElement(avatarCameraElement);
  });

  test('shows cr-dialog when open is true', async () => {
    assertEquals(
        null, avatarCameraElement.shadowRoot!.querySelector('cr-dialog'),
        'no cr-dialog present');

    avatarCameraElement.open = true;
    await waitAfterNextRender(avatarCameraElement);

    assertTrue(
        !!avatarCameraElement.shadowRoot!.querySelector('cr-dialog'),
        'cr-dialog present when open is true');
  });
}
