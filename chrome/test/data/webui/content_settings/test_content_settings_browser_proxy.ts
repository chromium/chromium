// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ContentSettingsPattern} from 'chrome://content-settings/content_settings.mojom-webui.js';
import type {PageHandlerInterface} from 'chrome://content-settings/content_settings_internals.mojom-webui.js';
import type {ContentSettingsType} from 'chrome://content-settings/content_settings_types.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestContentSettingsPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'readContentSettings',
      'contentSettingsPatternToString',
      'stringToContentSettingsPattern',
    ]);
  }

  readContentSettings(_type: ContentSettingsType) {
    this.methodCalled('readContentSettings', _type);
    return Promise.resolve({contentSettings: []});
  }

  contentSettingsPatternToString(_pattern: ContentSettingsPattern) {
    this.methodCalled('contentSettingsPatternToString', _pattern);
    return Promise.resolve({s: ''});
  }

  stringToContentSettingsPattern(_s: string) {
    this.methodCalled('stringToContentSettingsPattern', _s);
    return Promise.resolve({pattern: {} as ContentSettingsPattern});
  }
}
