// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OneDriveBrowserProxy, OneDrivePageCallbackRouter, OneDrivePageHandlerRemote, OneDrivePageRemote} from 'chrome://os-settings/os_settings.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  email?: string|null;
}

/**
 * A fake BrowserProxy implementation that enables switching out the real one to
 * mock various mojo responses.
 */
export class OneDriveTestBrowserProxy implements OneDriveBrowserProxy {
  handler: TestMock<OneDrivePageHandlerRemote>&OneDrivePageHandlerRemote;
  observer: OneDrivePageCallbackRouter;
  observerRemote: OneDrivePageRemote;

  constructor(options: ProxyOptions) {
    this.handler = TestMock.fromClass(OneDrivePageHandlerRemote);
    this.observer = new OneDrivePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();

    this.handler.setResultFor('getUserEmailAddress', {email: options.email});
    this.handler.setResultFor('connectToOneDrive', {success: true});
    this.handler.setResultFor('disconnectFromOneDrive', {success: true});
    this.handler.setResultFor('openOneDriveFolder', {success: true});
  }
}
