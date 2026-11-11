// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DialogArgs, DialogSpecificArgs} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {setInstallDelayForTesting} from 'chrome://cloud-upload/office_pwa_install_page.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  fileNames: string[];
  officeWebAppInstalled: boolean;
  installOfficeWebAppResult: boolean;
  odfsMounted: boolean;
  dialogSpecificArgs: DialogSpecificArgs;
  alwaysMoveOfficeFilesToDrive?: boolean|null;
  alwaysMoveOfficeFilesToOneDrive?: boolean|null;
  officeMoveConfirmationShownForDrive?: boolean|null;
  officeMoveConfirmationShownForOneDrive?: boolean|null;
}

export type TestCloudUploadPageHandler =
    TestMock<PageHandlerRemote>&PageHandlerRemote;

/**
 * Sets up a test PageHandlerRemote that enables mocking various mojo
 * responses.
 */
export function setupCloudUploadTestHandler(options: ProxyOptions):
    TestCloudUploadPageHandler {
  const handler = TestMock.fromClass(PageHandlerRemote);
  const args: DialogArgs = {
    fileNames: options.fileNames,
    dialogSpecificArgs: options.dialogSpecificArgs,
  };
  handler.setResultFor('getDialogArgs', {args: args});
  handler.setResultFor(
      'isOfficeWebAppInstalled', {installed: options.officeWebAppInstalled});
  handler.setResultFor(
      'installOfficeWebApp', {installed: options.installOfficeWebAppResult});
  handler.setResultFor('isODFSMounted', {mounted: options.odfsMounted});
  handler.setResultFor('signInToOneDrive', {success: true});
  handler.setResultFor('getAlwaysMoveOfficeFilesToDrive', {
    alwaysMove: options.alwaysMoveOfficeFilesToDrive,
  });
  handler.setResultFor('getAlwaysMoveOfficeFilesToOneDrive', {
    alwaysMove: options.alwaysMoveOfficeFilesToOneDrive,
  });
  handler.setResultFor('getOfficeMoveConfirmationShownForDrive', {
    moveConfirmationShown: options.officeMoveConfirmationShownForDrive,
  });
  handler.setResultFor('getOfficeMoveConfirmationShownForOneDrive', {
    moveConfirmationShown: options.officeMoveConfirmationShownForOneDrive,
  });
  browserProxyFactory.setInstance({handler});
  setInstallDelayForTesting(30);
  return handler;
}
