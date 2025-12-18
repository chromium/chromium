// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Size} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {PageHandlerInterface, ProfileEnablement, WebClientHandlerPendingReceiver, WebUiState} from 'chrome://glic/glic.mojom-webui.js';
import {PrepareForClientResult} from 'chrome://glic/glic.mojom-webui.js';
import {WebClientState} from 'chrome://glic/glic_api_impl/host/glic_api_host.js';
import type {ObservableValueReadOnly} from 'chrome://glic/observable.js';
import {ObservableValue} from 'chrome://glic/observable.js';
import type {WebviewControllerInterface} from 'chrome://glic/webview.js';

export class FakePageHandler implements PageHandlerInterface {
  prepareForClientResult: Promise<{result: PrepareForClientResult}> =
      Promise.resolve({result: PrepareForClientResult.kSuccess});

  createWebClient(_webClientReceiver: WebClientHandlerPendingReceiver): void {
    throw new Error('Method "createWebClient" not implemented.');
  }
  prepareForClient(): Promise<{result: PrepareForClientResult}> {
    return this.prepareForClientResult;
  }
  webviewCommitted(_url: Url): void {
    throw new Error('Method "webviewCommitted" not implemented.');
  }
  closePanel(): Promise<void> {
    throw new Error('Method "closePanel" not implemented.');
  }
  openProfilePickerAndClosePanel(): void {
    throw new Error('Method "openProfilePickerAndClosePanel" not implemented.');
  }
  openDisabledByAdminLinkAndClosePanel(): void {
    throw new Error(
        'Method "openDisabledByAdminLinkAndClosePanel" not implemented.');
  }
  signInAndClosePanel(): void {
    throw new Error('Method "signInAndClosePanel" not implemented.');
  }
  resizeWidget(_size: Size, _duration: TimeDelta): Promise<void> {
    throw new Error('Method "resizeWidget" not implemented.');
  }
  enableDragResize(_enabled: boolean): void {
    throw new Error('Method "enableDragResize" not implemented.');
  }
  webUiStateChanged(_newState: WebUiState): void {
    throw new Error('Method "webUiStateChanged" not implemented.');
  }
  getProfileEnablement(): Promise<{enablement: ProfileEnablement}> {
    throw new Error('Method "getProfileEnablement" not implemented.');
  }
}

export class FakeWebviewController implements WebviewControllerInterface {
  webClientState = ObservableValue.withValue(WebClientState.UNINITIALIZED);

  onLoadTimeOutCalled = false;

  webview: chrome.webviewTag.WebView =
      document.createElement('webview') as chrome.webviewTag.WebView;

  getWebClientState(): ObservableValueReadOnly<WebClientState> {
    return this.webClientState;
  }
  destroy(): void {}
  waitingOnPanelWillOpen(): boolean {
    return false;
  }
  onLoadTimeOut(): void {
    this.onLoadTimeOutCalled = true;
  }
}
