// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SmbBrowserProxy, SmbMountResult} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSmbBrowserProxy extends TestBrowserProxy implements
    SmbBrowserProxy {
  smbMountResult = SmbMountResult.SUCCESS;
  anySmbMounted = false;

  constructor() {
    super([
      'hasAnySmbMountedBefore',
    ]);
  }

  smbMount(
      smbUrl: string, smbName: string, username: string, password: string,
      authMethod: string, shouldOpenFileManagerAfterMount: boolean,
      saveCredentials: boolean): Promise<SmbMountResult> {
    this.methodCalled(
        'smbMount', smbUrl, smbName, username, password, authMethod,
        shouldOpenFileManagerAfterMount, saveCredentials);
    return Promise.resolve(this.smbMountResult);
  }

  startDiscovery(): void {
    this.methodCalled('startDiscovery');
  }

  updateCredentials(mountId: string, username: string, password: string): void {
    this.methodCalled('updateCredentials', mountId, username, password);
  }

  hasAnySmbMountedBefore(): Promise<boolean> {
    this.methodCalled('hasAnySmbMountedBefore');
    return Promise.resolve(this.anySmbMounted);
  }
}
