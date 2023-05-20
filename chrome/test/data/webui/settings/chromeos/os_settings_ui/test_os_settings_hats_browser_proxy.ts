// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSettingsHatsBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOsSettingsHatsBrowserProxy extends TestBrowserProxy implements
    OsSettingsHatsBrowserProxy {
  constructor() {
    super(['sendSettingsHats', 'settingsUsedSearch']);
  }

  sendSettingsHats(): void {
    this.methodCalled('sendSettingsHats');
  }

  settingsUsedSearch(): void {
    this.methodCalled('settingsUsedSearch');
  }
}
