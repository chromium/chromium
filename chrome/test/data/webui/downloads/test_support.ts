// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconLoader, MojomData, PageHandlerInterface} from 'chrome://downloads/downloads.js';
import {DangerType, SafeBrowsingState, State, TailoredWarningType} from 'chrome://downloads/downloads.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class FakePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private eligibleForEsbPromo_: boolean = false;

  constructor() {
    super([
      'discardDangerous',
      'isEligibleForEsbPromo',
      'logEsbPromotionRowViewed',
      'openEsbSettings',
      'recordCancelBypassWarningDialog',
      'recordOpenBypassWarningDialog',
      'remove',
      'saveDangerousFromDialogRequiringGesture',
      'saveSuspiciousRequiringGesture',
    ]);
  }

  recordCancelBypassWarningDialog(id: string) {
    this.methodCalled('recordCancelBypassWarningDialog', id);
  }

  recordOpenBypassWarningDialog(id: string) {
    this.methodCalled('recordOpenBypassWarningDialog', id);
  }

  remove(id: string) {
    this.methodCalled('remove', id);
  }

  discardDangerous(id: string) {
    this.methodCalled('discardDangerous', id);
  }

  saveDangerousFromDialogRequiringGesture(id: string) {
    this.methodCalled('saveDangerousFromDialogRequiringGesture', id);
  }

  saveSuspiciousRequiringGesture(id: string) {
    this.methodCalled('saveSuspiciousRequiringGesture', id);
  }

  openEsbSettings() {
    this.methodCalled('openEsbSettings');
  }

  logEsbPromotionRowViewed() {
    this.methodCalled('logEsbPromotionRowViewed');
  }

  getDownloads(_searchTerms: string[]) {}
  openFileRequiringGesture(_id: string) {}
  drag(_id: string) {}
  retryDownload(_id: string) {}
  show(_id: string) {}
  pause(_id: string) {}
  resume(_id: string) {}
  undo() {}
  cancel(_id: string) {}
  clearAll() {}
  openDownloadsFolderRequiringGesture() {}
  openDuringScanningRequiringGesture(_id: string) {}
  reviewDangerousRequiringGesture(_id: string) {}
  deepScan(_id: string) {}
  bypassDeepScanRequiringGesture(_id: string) {}
  isEligibleForEsbPromo(): Promise<{result: boolean}> {
    this.methodCalled('isEligibleForEsbPromo');
    return Promise.resolve({result: this.eligibleForEsbPromo_});
  }
  setEligbleForEsbPromo(eligible: boolean) {
    this.eligibleForEsbPromo_ = eligible;
  }
}

export class TestIconLoader extends TestBrowserProxy implements IconLoader {
  private shouldIconsLoad_: boolean = true;

  constructor() {
    super(['loadIcon']);
  }

  setShouldIconsLoad(shouldIconsLoad: boolean) {
    this.shouldIconsLoad_ = shouldIconsLoad;
  }

  loadIcon(_imageEl: HTMLImageElement, filePath: string) {
    this.methodCalled('loadIcon', filePath);
    return Promise.resolve(this.shouldIconsLoad_);
  }
}

export function createDownload(config?: Partial<MojomData>): MojomData {
  return Object.assign(
      {
        byExtId: '',
        byExtName: '',
        dangerType: DangerType.kNoApplicableDangerType,
        dateString: '',
        fileExternallyRemoved: false,
        fileName: 'download 1',
        filePath: '/some/file/path',
        fileUrl: 'file:///some/file/path',
        hideDate: false,
        id: '123',
        isDangerous: false,
        isInsecure: false,
        isReviewable: false,
        lastReasonText: '',
        otr: false,
        percent: 100,
        progressStatusText: '',
        resume: false,
        retry: false,
        return: false,
        shouldShowIncognitoWarning: false,
        showInFolderText: '',
        sinceString: 'Today',
        started: Date.now() - 10000,
        state: State.kComplete,
        tailoredWarningType:
            TailoredWarningType.kNoApplicableTailoredWarningType,
        total: -1,
        url: 'http://permission.site',
        displayInitiatorOrigin: 'http://permission.site',
        safeBrowsingState: SafeBrowsingState.kStandardProtection,
        hasSafeBrowsingVerdict: true,
      },
      config || {});
}
