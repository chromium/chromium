// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HatsBrowserProxy} from 'chrome://settings/settings.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {HatsBrowserProxy} */
export class TestHatsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'tryShowSurvey',
      'tryShowPrivacySandboxSurvey',
    ]);
  }

  /** @override*/
  tryShowSurvey() {
    this.methodCalled('tryShowSurvey');
  }

  /** @override*/
  tryShowPrivacySandboxSurvey() {
    this.methodCalled('tryShowPrivacySandboxSurvey');
  }
}
