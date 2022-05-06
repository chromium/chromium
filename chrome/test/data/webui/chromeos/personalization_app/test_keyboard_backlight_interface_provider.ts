// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor, KeyboardBacklightProviderInterface} from 'chrome://personalization/trusted/personalization_app.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestKeyboardBacklightProvider extends
    TestBrowserProxy<KeyboardBacklightProviderInterface> implements
        KeyboardBacklightProviderInterface {
  constructor() {
    super([
      'setBacklightColor',
    ]);
  }

  setBacklightColor(backlightColor: BacklightColor) {
    this.methodCalled('setBacklightColor', backlightColor);
  }
}
