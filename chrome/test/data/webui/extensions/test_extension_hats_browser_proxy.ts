// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionsHatsBrowserProxy} from 'chrome://extensions/extensions.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestExtensionsHatsBrowserProxy extends TestBrowserProxy implements
    ExtensionsHatsBrowserProxy {
  constructor() {
    super([
      'triggerSurvey',
      'extensionKeptAction',
      'extensionRemovedAction',
      'nonTriggerExtensionRemovedAction',
      'removeAllAction',
    ]);
  }

  triggerSurvey() {
    this.methodCalled('triggerSurvey');
  }

  extensionKeptAction() {
    this.methodCalled('extensionKeptAction');
  }

  extensionRemovedAction() {
    this.methodCalled('extensionRemovedAction');
  }

  nonTriggerExtensionRemovedAction() {
    this.methodCalled('nonTriggerExtensionRemovedAction');
  }

  removeAllAction(numberOfExtensionsRemoved: number) {
    this.methodCalled('removeAllAction', [numberOfExtensionsRemoved]);
  }
}
