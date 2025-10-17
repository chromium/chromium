// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfViewerElement, PdfViewerPrivateProxy} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PdfViewerPrivateProxyImpl} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const SaveToDriveStatus = chrome.pdfViewerPrivate.SaveToDriveStatus;
const SaveToDriveErrorType = chrome.pdfViewerPrivate.SaveToDriveErrorType;

export class TestPdfViewerPrivateProxy extends TestBrowserProxy implements
    PdfViewerPrivateProxy {
  onSaveToDriveProgress: FakeChromeEvent;
  private streamUrl_: string = '';

  constructor() {
    super([
      'saveToDrive',
      'setPdfDocumentTitle',
    ]);

    this.onSaveToDriveProgress = new FakeChromeEvent();
  }

  sendSaveToDriveProgress(
      progress: chrome.pdfViewerPrivate.SaveToDriveProgress): void {
    this.onSaveToDriveProgress.callListeners(this.streamUrl_, progress);
  }

  saveToDrive(saveRequestType?: chrome.pdfViewerPrivate.SaveRequestType): void {
    this.methodCalled('saveToDrive', saveRequestType);
  }

  setPdfDocumentTitle(title: string): void {
    this.methodCalled('setPdfDocumentTitle', title);
  }

  setStreamUrl(streamUrl: string): void {
    this.streamUrl_ = streamUrl;
  }

  sendQuotaExceededError(accountIsManaged: boolean): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.QUOTA_EXCEEDED,
      accountEmail: 'test@gmail.com',
      accountIsManaged: accountIsManaged,
    });
  }

  sendSessionTimeoutError(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.OAUTH_ERROR,
    });
  }

  sendUploadInProgress(uploadedBytes: number, fileSizeBytes: number): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_IN_PROGRESS,
      errorType: SaveToDriveErrorType.NO_ERROR,
      uploadedBytes: uploadedBytes,
      fileSizeBytes: fileSizeBytes,
      fileMetadata: 'uploading, 2 minutes left',
    });
  }

  sendUploadCompleted(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.UPLOAD_COMPLETED,
      errorType: SaveToDriveErrorType.NO_ERROR,
      driveItemId: 'test-drive-item-id',
      parentFolderName: 'test-parent-folder-name',
      fileName: 'save_to_drive_test.pdf',
      accountEmail: 'test@gmail.com',
    });
  }

  sendUninitializedState(): void {
    this.sendSaveToDriveProgress({
      status: SaveToDriveStatus.NOT_STARTED,
      errorType: SaveToDriveErrorType.NO_ERROR,
    });
  }
}

export function setUpTestPdfViewerPrivateProxy(viewer: PdfViewerElement):
    TestPdfViewerPrivateProxy {
  const privateProxy = new TestPdfViewerPrivateProxy();
  privateProxy.setStreamUrl(viewer.getStreamUrlForTesting());
  PdfViewerPrivateProxyImpl.setInstance(privateProxy);
  viewer.setOnSaveToDriveProgressListenerForTesting();
  return privateProxy;
}
