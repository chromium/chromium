// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageHandlerInterface, LensPageRemote, SemanticEvent, UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {LensPageCallbackRouter} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {ClickModifiers} from 'chrome-untrusted://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the LensPageHandler used to verify calls to the browser from
 * WebUI.
 */
export class TestLensOverlayPageHandler extends TestBrowserProxy implements
    LensPageHandlerInterface {
  constructor() {
    super([
      'activityRequestedByOverlay',
      'closeRequestedByOverlayCloseButton',
      'closeRequestedByOverlayBackgroundClick',
      'addBackgroundBlur',
      'closeSearchBubble',
      'closePreselectionBubble',
      'feedbackRequestedByOverlay',
      'getOverlayInvocationSource',
      'infoRequestedByOverlay',
      'issueLensRegionRequest',
      'issueLensObjectRequest',
      'issueTextSelectionRequest',
      'issueTranslateSelectionRequest',
      'issueTranslateFullPageRequest',
      'issueEndTranslateModeRequest',
      'notifyOverlayInitialized',
      'copyText',
      'copyImage',
      'saveAsImage',
      'recordUkmAndTaskCompletionForLensOverlayInteraction',
      'recordLensOverlaySemanticEvent',
      'maybeShowTranslateFeaturePromo',
      'maybeCloseTranslateFeaturePromo',
    ]);
  }

  activityRequestedByOverlay(clickModifiers: ClickModifiers) {
    this.methodCalled('activityRequestedByOverlay', clickModifiers);
  }

  closeRequestedByOverlayCloseButton() {
    this.methodCalled('closeRequestedByOverlayCloseButton');
  }

  closeRequestedByOverlayBackgroundClick() {
    this.methodCalled('closeRequestedByOverlayBackgroundClick');
  }

  addBackgroundBlur() {
    this.methodCalled('addBackgroundBlur');
  }

  closeSearchBubble() {
    this.methodCalled('closeSearchBubble');
  }

  closePreselectionBubble() {
    this.methodCalled('closePreselectionBubble');
  }

  feedbackRequestedByOverlay() {
    this.methodCalled('feedbackRequestedByOverlay');
  }

  getOverlayInvocationSource(): Promise<{invocationSource: string}> {
    this.methodCalled('getOverlayInvocationSource');
    return Promise.resolve({invocationSource: 'AppMenu'});
  }

  infoRequestedByOverlay(clickModifiers: ClickModifiers) {
    this.methodCalled('infoRequestedByOverlay', clickModifiers);
  }

  issueLensRegionRequest(rect: CenterRotatedBox, isClick: boolean) {
    this.methodCalled('issueLensRegionRequest', rect, isClick);
  }

  issueLensObjectRequest(rect: CenterRotatedBox, isMaskClick: boolean) {
    this.methodCalled('issueLensObjectRequest', rect, isMaskClick);
  }

  issueTextSelectionRequest(query: string) {
    this.methodCalled('issueTextSelectionRequest', query);
  }

  issueTranslateSelectionRequest(query: string) {
    this.methodCalled('issueTranslateSelectionRequest', query);
  }

  issueTranslateFullPageRequest(
      sourceLanguage: string, targetLanguage: string) {
    this.methodCalled(
        'issueTranslateFullPageRequest', sourceLanguage, targetLanguage);
  }

  issueEndTranslateModeRequest() {
    this.methodCalled('issueEndTranslateModeRequest');
  }

  notifyOverlayInitialized() {
    this.methodCalled('notifyOverlayInitialized');
  }

  copyText(text: string) {
    this.methodCalled('copyText', text);
  }

  copyImage(region: CenterRotatedBox) {
    this.methodCalled('copyImage', region);
  }

  saveAsImage(region: CenterRotatedBox) {
    this.methodCalled('saveAsImage', region);
  }

  recordUkmAndTaskCompletionForLensOverlayInteraction(userAction: UserAction) {
    this.methodCalled(
        'recordUkmAndTaskCompletionForLensOverlayInteraction', userAction);
  }

  recordLensOverlaySemanticEvent(semanticEvent: SemanticEvent) {
    this.methodCalled('recordLensOverlaySemanticEvent', semanticEvent);
  }

  maybeShowTranslateFeaturePromo() {
    this.methodCalled('maybeShowTranslateFeaturePromo');
  }

  maybeCloseTranslateFeaturePromo() {
    this.methodCalled('maybeCloseTranslateFeaturePromo');
  }
}

/**
 * Test version of the BrowserProxy used in connecting Lens Overlay to the
 * browser on start up.
 */
export class TestLensOverlayBrowserProxy implements BrowserProxy {
  callbackRouter: LensPageCallbackRouter = new LensPageCallbackRouter();
  handler: TestLensOverlayPageHandler = new TestLensOverlayPageHandler();
  page: LensPageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();
}
