// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconLoader, MojomData, PageHandlerInterface, PageRemote} from 'chrome://downloads/downloads.js';
import {DangerType, PageCallbackRouter, SafeBrowsingState, State, TailoredWarningType} from 'chrome://downloads/downloads.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDownloadsProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.handler = new FakePageHandler(this.callbackRouterRemote);
  }
}

class FakePageHandler implements PageHandlerInterface {
  private eligibleForEsbPromo_: boolean = false;
  private callbackRouterRemote_: PageRemote;
  private callTracker_: TestBrowserProxy = new TestBrowserProxy([
    'discardDangerous',
    'isEligibleForEsbPromo',
    'logEsbPromotionRowViewed',
    'openEsbSettings',
    'recordCancelBypassWarningDialog',
    'recordCancelBypassWarningInterstitial',
    'recordOpenBypassWarningDialog',
    'recordOpenBypassWarningInterstitial',
    'recordOpenSurveyOnDangerousInterstitial',
    'remove',
    'saveDangerousFromDialogRequiringGesture',
    'saveDangerousFromInterstitialNeedGesture',
    'saveSuspiciousRequiringGesture',
  ]);

  constructor(callbackRouterRemote: PageRemote) {
    this.callbackRouterRemote_ = callbackRouterRemote;
  }

  whenCalled(methodName: string): Promise<void> {
    return this.callTracker_.whenCalled(methodName);
  }

  recordCancelBypassWarningDialog(id: string) {
    this.callTracker_.methodCalled('recordCancelBypassWarningDialog', id);
  }

  recordCancelBypassWarningInterstitial(id: string) {
    this.callTracker_.methodCalled('recordCancelBypassWarningInterstitial', id);
  }

  recordOpenBypassWarningDialog(id: string) {
    this.callTracker_.methodCalled('recordOpenBypassWarningDialog', id);
  }

  recordOpenBypassWarningInterstitial(id: string) {
    this.callTracker_.methodCalled('recordOpenBypassWarningInterstitial', id);
  }

  recordOpenSurveyOnDangerousInterstitial(id: string) {
    this.callTracker_.methodCalled(
        'recordOpenSurveyOnDangerousInterstitial', id);
  }

  async remove(id: string) {
    this.callbackRouterRemote_.removeItem(0);
    await this.callbackRouterRemote_.$.flushForTesting();
    this.callTracker_.methodCalled('remove', id);
  }

  discardDangerous(id: string) {
    this.callTracker_.methodCalled('discardDangerous', id);
  }

  saveDangerousFromDialogRequiringGesture(id: string) {
    this.callTracker_.methodCalled(
        'saveDangerousFromDialogRequiringGesture', id);
  }

  saveDangerousFromInterstitialNeedGesture(id: string) {
    this.callTracker_.methodCalled(
        'saveDangerousFromInterstitialNeedGesture', id);
  }

  saveSuspiciousRequiringGesture(id: string) {
    this.callTracker_.methodCalled('saveSuspiciousRequiringGesture', id);
  }

  openEsbSettings() {
    this.callTracker_.methodCalled('openEsbSettings');
  }

  logEsbPromotionRowViewed() {
    this.callTracker_.methodCalled('logEsbPromotionRowViewed');
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
  async isEligibleForEsbPromo(): Promise<{result: boolean}> {
    this.callTracker_.methodCalled('isEligibleForEsbPromo');
    return {result: this.eligibleForEsbPromo_};
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
        accountEmail: '',
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
        url: stringToMojoUrl('http://permission.site'),
        displayUrl: stringToMojoString16('http://permission.site'),
        referrerUrl: stringToMojoUrl('http://permission.site'),
        displayReferrerUrl: stringToMojoString16('http://permission.site'),
        safeBrowsingState: SafeBrowsingState.kStandardProtection,
        hasSafeBrowsingVerdict: true,
      },
      config || {});
}
