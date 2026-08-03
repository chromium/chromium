// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, assertEquals, assertRejects, assertTrue, testMain} from './browser_test_base.js';

class GlicPermissionEnforcementApiTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testMicrophonePermissionTestDeny() {
    try {
      await navigator.mediaDevices.getUserMedia({audio: true});
      throw new Error('Microphone access should have been denied');
    } catch (e) {
      const error = e as Error;
      assertEquals('NotAllowedError', error.name);
    }
  }

  async testMicrophonePermissionTestAllow() {
    const stream = await navigator.mediaDevices.getUserMedia({audio: true});
    assertTrue(!!stream);
    for (const track of stream.getTracks()) {
      track.stop();
    }
  }

  async testTabContextPermissionTestDeny() {
    await assertRejects(this.host.getContextFromFocusedTab!({}), {
      withErrorMessage: 'tabContext failed: permission denied',
    });
  }

  async testTabContextPermissionTestAllow() {
    const result = await this.host.getContextFromFocusedTab!({});
    assertTrue(!!result);
  }

  async testLocationPermissionTestDeny() {
    const error =
        await new Promise<GeolocationPositionError|null>((resolve) => {
          navigator.geolocation.getCurrentPosition(
              () => resolve(null), (err) => resolve(err));
        });
    assertTrue(!!error);
    assertEquals(GeolocationPositionError.PERMISSION_DENIED, error.code);
  }

  async testLocationPermissionTestAllow() {
    const position: GeolocationPosition =
        await new Promise((resolve, reject) => {
          navigator.geolocation.getCurrentPosition(resolve, reject);
        });
    assertTrue(!!position);
  }
}

testMain([
  GlicPermissionEnforcementApiTest,
]);
