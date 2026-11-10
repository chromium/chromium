// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OneDrivePageRemote} from 'chrome://os-settings/os_settings.js';
import {OneDrivePageHandlerRemote, oneDriveProxyFactory} from 'chrome://os-settings/os_settings.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  email?: string|null;
}

export interface OneDriveTestBrowserProxy {
  handler: TestMock<OneDrivePageHandlerRemote>&OneDrivePageHandlerRemote;
  observerRemote: OneDrivePageRemote;
}

export function setupOneDriveTestProxy(options: ProxyOptions):
    OneDriveTestBrowserProxy {
  const handler = TestMock.fromClass(OneDrivePageHandlerRemote);
  handler.setResultFor('getUserEmailAddress', {email: options.email});
  handler.setResultFor('connectToOneDrive', {success: true});
  handler.setResultFor('disconnectFromOneDrive', {success: true});
  handler.setResultFor('openOneDriveFolder', {success: true});

  const {instance, remote} = oneDriveProxyFactory.createForTest(handler);
  oneDriveProxyFactory.setInstance(instance);
  return {handler, observerRemote: remote};
}
