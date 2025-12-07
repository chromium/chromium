// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {ClientApi} from 'chrome-untrusted://class-tools-remote-display/app/boca_receiver_app.js';
import {BrowserProxyImpl} from 'chrome-untrusted://class-tools-remote-display/app/browser_proxy.js';
import type {BrowserProxy} from 'chrome-untrusted://class-tools-remote-display/app/browser_proxy.js';
import {UntrustedPageCallbackRouter} from 'chrome-untrusted://class-tools-remote-display/mojom/boca_receiver.mojom-webui.js';
import type {UntrustedPageRemote, UserInfo} from 'chrome-untrusted://class-tools-remote-display/mojom/boca_receiver.mojom-webui.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeBrowserProxy implements BrowserProxy {
  callbackRouter: UntrustedPageCallbackRouter;
  pageRemote: UntrustedPageRemote;

  constructor() {
    this.callbackRouter = new UntrustedPageCallbackRouter();
    this.pageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

interface ConnectingParam {
  initiator: UserInfo;
  presenter: UserInfo|null;
}

suite('BocaReceiverAppTest', () => {
  let browserProxy: FakeBrowserProxy;
  let app: ClientApi;

  setup(function() {
    browserProxy = new FakeBrowserProxy();
    BrowserProxyImpl.setInstanceForTest(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const bocaReceiverElement = document.createElement('boca-receiver-app');
    document.body.appendChild(bocaReceiverElement);
    app = bocaReceiverElement as unknown as ClientApi;
    window.document.dispatchEvent(
        new Event('DOMContentLoaded', {bubbles: true}));
  });

  test('init receiver info', async function() {
    const expectedReceiverInfo = {id: 'H2O2'};
    const promise = new Promise(resolve => {
      app.onInitReceiverInfo = (receiverInfo) => {
        resolve(receiverInfo);
      };
    });
    browserProxy.pageRemote.onInitReceiverInfo(expectedReceiverInfo);
    const actualReceiverInfo = await promise;
    assertDeepEquals(actualReceiverInfo, expectedReceiverInfo);
  });

  test('init receiver error', async function() {
    const promise = new Promise<void>(resolve => {
      app.onInitReceiverError = () => {
        resolve();
      };
    });
    browserProxy.pageRemote.onInitReceiverError();
    await promise;
  });

  test('frame received', async function() {
    const imageInfo = {
      alphaType: 1,
      width: 2,
      height: 1,
      colorTransferFunction: null,
      colorToXyzMatrix: null,
    };
    const pixelData = [255, 0, 128];
    const expectedImage = {imageInfo, pixelData: {bytes: pixelData}};
    const promise = new Promise(resolve => {
      app.onFrameReceived = (image) => {
        resolve(image);
      };
    });
    browserProxy.pageRemote.onFrameReceived(expectedImage);
    const actualImage = await promise;
    assertDeepEquals(actualImage, expectedImage);
  });

  test('connecting', async function() {
    const expectedInitiator = {name: 'teacher'};
    const expectedPresenter = {name: 'student'};
    const promise = new Promise<ConnectingParam>(resolve => {
      app.onConnecting = (initiator, presenter) => {
        resolve({'initiator': initiator, 'presenter': presenter});
      };
    });
    browserProxy.pageRemote.onConnecting(expectedInitiator, expectedPresenter);
    const params = await promise;
    assertDeepEquals(params.initiator, expectedInitiator);
    assertDeepEquals(params.presenter, expectedPresenter);
  });

  test('connection closed', async function() {
    const expectedReason = 2;
    const promise = new Promise(resolve => {
      app.onConnectionClosed = (reason) => {
        resolve(reason);
      };
    });
    browserProxy.pageRemote.onConnectionClosed(expectedReason);
    const actualReason = await promise;
    assertEquals(actualReason, expectedReason);
  });
});
