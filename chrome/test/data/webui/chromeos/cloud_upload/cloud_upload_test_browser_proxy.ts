// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogArgs, DialogPage, DialogTask, PageHandlerRemote} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  fileName?: string|null;
  officeWebAppInstalled: boolean;
  installOfficeWebAppResult: boolean;
  odfsMounted: boolean;
  dialogPage: DialogPage;
  tasks?: DialogTask[]|null;
  firstTimeSetup?: boolean|null;
}

/**
 * A test CloudUploadBrowserProxy implementation that enables to mock various
 * mojo responses.
 */
export class CloudUploadTestBrowserProxy implements CloudUploadBrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  constructor(options: ProxyOptions) {
    this.handler = TestMock.fromClass(PageHandlerRemote);
    const args: DialogArgs = {
      fileNames: [],
      dialogPage: options.dialogPage,
      tasks: [],
      firstTimeSetup: true,
    };
    if (options.fileName != null) {
      args.fileNames.push(options.fileName);
    }
    if (options.tasks != null) {
      args.tasks = options.tasks;
    }
    if (options.firstTimeSetup != null) {
      args.firstTimeSetup = options.firstTimeSetup;
    }
    this.handler.setResultFor('getDialogArgs', {args: args});
    this.handler.setResultFor(
        'isOfficeWebAppInstalled', {installed: options.officeWebAppInstalled});
    this.handler.setResultFor(
        'installOfficeWebApp', {installed: options.installOfficeWebAppResult});
    this.handler.setResultFor('isODFSMounted', {mounted: options.odfsMounted});
    this.handler.setResultFor('signInToOneDrive', {success: true});
  }

  isTest() {
    return true;
  }
}
