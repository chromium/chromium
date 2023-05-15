// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogArgs, DialogPage, DialogTask, OperationType, PageHandlerRemote} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  fileName?: string|null;
  officeWebAppInstalled: boolean;
  installOfficeWebAppResult: boolean;
  odfsMounted: boolean;
  dialogPage: DialogPage;
  localTasks?: DialogTask[]|null;
  firstTimeSetup?: boolean|null;
  alwaysMoveOfficeFilesToDrive?: boolean|null;
  alwaysMoveOfficeFilesToOneDrive?: boolean|null;
  officeMoveConfirmationShownForDrive?: boolean|null;
  officeMoveConfirmationShownForOneDrive?: boolean|null;
  operationType: OperationType;
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
      localTasks: [],
      firstTimeSetup: true,
      operationType: options.operationType,
    };
    if (options.fileName != null) {
      args.fileNames.push(options.fileName);
    }
    if (options.localTasks != null) {
      args.localTasks = options.localTasks;
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
    this.handler.setResultFor('getAlwaysMoveOfficeFilesToDrive', {
      alwaysMove: options.alwaysMoveOfficeFilesToDrive,
    });
    this.handler.setResultFor('getAlwaysMoveOfficeFilesToOneDrive', {
      alwaysMove: options.alwaysMoveOfficeFilesToOneDrive,
    });
    this.handler.setResultFor('getOfficeMoveConfirmationShownForDrive', {
      moveConfirmationShown: options.officeMoveConfirmationShownForDrive,
    });
    this.handler.setResultFor('getOfficeMoveConfirmationShownForOneDrive', {
      moveConfirmationShown: options.officeMoveConfirmationShownForOneDrive,
    });
  }

  isTest() {
    return true;
  }
}
