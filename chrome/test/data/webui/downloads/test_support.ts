// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconLoader, MojomData, PageHandlerInterface, PageRemote} from 'chrome://downloads/downloads.js';
import {DangerType, PageCallbackRouter, SafeBrowsingState, State} from 'chrome://downloads/downloads.js';
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
  private callbackRouterRemote_: PageRemote;
  private callTracker_: TestBrowserProxy = new TestBrowserProxy([
    'recordCancelBypassWarningPrompt',
    'recordOpenBypassWarningPrompt',
    'remove',
    'saveDangerousFromPromptRequiringGesture',
    'saveDangerousRequiringGesture',
    'saveSuspiciousRequiringGesture',
  ]);

  constructor(callbackRouterRemote: PageRemote) {
    this.callbackRouterRemote_ = callbackRouterRemote;
  }

  whenCalled(methodName: string): Promise<void> {
    return this.callTracker_.whenCalled(methodName);
  }

  recordCancelBypassWarningPrompt(id: string) {
    this.callTracker_.methodCalled('recordCancelBypassWarningPrompt', id);
  }

  recordOpenBypassWarningPrompt(id: string) {
    this.callTracker_.methodCalled('recordOpenBypassWarningPrompt', id);
  }

  async remove(id: string) {
    this.callbackRouterRemote_.removeItem(0);
    await this.callbackRouterRemote_.$.flushForTesting();
    this.callTracker_.methodCalled('remove', id);
  }

  saveDangerousFromPromptRequiringGesture(id: string) {
    this.callTracker_.methodCalled(
        'saveDangerousFromPromptRequiringGesture', id);
  }

  saveDangerousRequiringGesture(id: string) {
    this.callTracker_.methodCalled('saveDangerousRequiringGesture', id);
  }

  saveSuspiciousRequiringGesture(id: string) {
    this.callTracker_.methodCalled('saveSuspiciousRequiringGesture', id);
  }

  getDownloads(_searchTerms: string[]) {}
  openFileRequiringGesture(_id: string) {}
  drag(_id: string) {}
  acceptIncognitoWarning(_id: string) {}
  discardDangerous(_id: string) {}
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
  openEsbSettings() {}
  async isEligibleForEsbPromo(): Promise<{result: boolean}> {
    return {result: false};
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
        total: -1,
        url: stringToMojoUrl('http://permission.site'),
        displayUrl: stringToMojoString16('http://permission.site'),
        safeBrowsingState: SafeBrowsingState.kStandardProtection,
        hasSafeBrowsingVerdict: true,
      },
      config || {});
}
