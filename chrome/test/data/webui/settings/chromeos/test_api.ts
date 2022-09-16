// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';

class OSSettingsDriver implements OSSettingsDriverInterface {}

// Passes an OSSettingsDriver remote to the browser process.
export async function register(): Promise<void> {
  const browserProcess = OSSettingsBrowserProcess.getRemote();
  const receiver = new OSSettingsDriverReceiver(new OSSettingsDriver());
  const remote = receiver.$.bindNewPipeAndPassRemote();
  await browserProcess.registerOSSettingsDriver(remote);
}
