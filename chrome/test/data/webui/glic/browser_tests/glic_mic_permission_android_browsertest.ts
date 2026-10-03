// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path:
// chrome/browser/glic/android/glic_mic_permission_android_browsertest.cc

import {ApiTestFixtureBase, assertEquals, assertTrue, testMain} from './browser_test_base.js';

async function expectMicAllowed() {
  const stream = await navigator.mediaDevices.getUserMedia({audio: true});
  assertTrue(!!stream);
  for (const track of stream.getTracks()) {
    track.stop();
  }
}

async function expectMicDenied() {
  try {
    await navigator.mediaDevices.getUserMedia({audio: true});
  } catch (e) {
    assertEquals('NotAllowedError', (e as Error).name);
    return;
  }
  throw new Error('Microphone access should have been denied');
}

class GlicMicPermissionAndroidApiTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testMicAllowedWithOsPermission() {
    await expectMicAllowed();
  }

  async testMicAllowedAfterDialogAndOsPromptAccepted() {
    await expectMicAllowed();
  }

  async testMicDeniedAfterDialogDeclined() {
    await expectMicDenied();
  }

  async testMicDeniedAfterOsPromptDenied() {
    await expectMicDenied();
  }
}

testMain([
  GlicMicPermissionAndroidApiTest,
]);
