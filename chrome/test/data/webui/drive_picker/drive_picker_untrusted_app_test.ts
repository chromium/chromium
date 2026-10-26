// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://drive-picker-host/strings.m.js';
import 'chrome-untrusted://drive-picker-host/app.js';

import type {ProtoWrapper} from '//resources/mojo/mojo/public/mojom/base/proto_wrapper.mojom-webui.js';
import {HANDSHAKE_TIMEOUT_MS} from 'chrome-untrusted://drive-picker-host/app.js';
import type {DrivePickerHostUntrustedAppElement} from 'chrome-untrusted://drive-picker-host/app.js';
import {BrowserProxyImpl} from 'chrome-untrusted://drive-picker-host/browser_proxy.js';
import {PageCallbackRouter} from 'chrome-untrusted://drive-picker-host/drive_picker_host_untrusted.mojom-webui.js';
import type {PageHandlerInterface, PageHandlerRemote, PageRemote} from 'chrome-untrusted://drive-picker-host/drive_picker_host_untrusted.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  constructor() {
    super([
      'onConsentKitIframeMessage',
      'onConsentKitPrivacyFlowResult',
      'onConsentKitError',
      'onShowErrorDialog',
      'onHideErrorDialog',
    ]);
  }

  onConsentKitIframeMessage(messageWrapper: ProtoWrapper) {
    this.methodCalled('onConsentKitIframeMessage', messageWrapper);
  }

  onConsentKitPrivacyFlowResult(resultWrapper: ProtoWrapper) {
    this.methodCalled('onConsentKitPrivacyFlowResult', resultWrapper);
  }

  onConsentKitError(errorMessage: string) {
    this.methodCalled('onConsentKitError', errorMessage);
  }

  onShowErrorDialog() {
    this.methodCalled('onShowErrorDialog');
  }

  onHideErrorDialog() {
    this.methodCalled('onHideErrorDialog');
  }
}

suite('DrivePickerHostUntrustedAppTest', function() {
  let app: DrivePickerHostUntrustedAppElement;
  let handler: TestPageHandler;
  let mockTimer: MockTimer|null = null;

  setup(async function() {
    if (window.trustedTypes) {
      document.body.innerHTML = window.trustedTypes.emptyHTML;
    } else {
      document.body.innerHTML = '';
    }
    handler = new TestPageHandler();
    BrowserProxyImpl.setInstance({
      handler: handler as unknown as PageHandlerRemote,
      callbackRouter: new PageCallbackRouter(),
    });
    app = document.createElement('drive-picker-host-untrusted-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  teardown(function() {
    mockTimer?.uninstall();
    mockTimer = null;
  });

  test('AppIsAttached', function() {
    assertTrue(app.isConnected);
  });

  test('LoadsConsentKitUrlIframe', async function() {
    const testUrl = 'https://consent.google.com/primitive?hl=en';

    const browserProxy = BrowserProxyImpl.getInstance();
    const callbackRouterRemote: PageRemote =
        browserProxy.callbackRouter.$.bindNewPipeAndPassRemote();

    callbackRouterRemote.loadConsentKitUrl(testUrl);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!app.shadowRoot);
    const container = app.shadowRoot.querySelector('#picker-container');
    assertTrue(!!container);
    if (!container) {
      return;
    }
    const iframe = container.querySelector('iframe');
    assertTrue(!!iframe);
    if (!iframe) {
      return;
    }
    assertEquals(testUrl, iframe.src);
  });

  test('TryAgainNotifiesBrowserAndReloadsConsentKit', async function() {
    const testUrl = 'https://consent.google.com/primitive?hl=en';
    mockTimer = new MockTimer();
    mockTimer.install();
    const callbackRouterRemote: PageRemote =
        BrowserProxyImpl.getInstance()
            .callbackRouter.$.bindNewPipeAndPassRemote();
    callbackRouterRemote.loadConsentKitUrl(testUrl);
    await callbackRouterRemote.$.flushForTesting();

    // ConsentKit never sends the handshake, so the error screen is shown.
    mockTimer.tick(HANDSHAKE_TIMEOUT_MS);
    await app.updateComplete;
    assertEquals(1, handler.getCallCount('onShowErrorDialog'));
    const container =
        app.shadowRoot.querySelector<HTMLElement>('#picker-container');
    assertTrue(!!container);
    assertTrue(container.hidden);
    const oldIframe = container.querySelector('iframe');
    assertTrue(!!oldIframe);
    const tryAgainButton = app.shadowRoot.querySelector<HTMLElement>(
        '#error-screen .action-button');
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    await app.updateComplete;

    // The browser is told to leave the error size and ConsentKit is reloaded.
    assertEquals(1, handler.getCallCount('onHideErrorDialog'));
    assertFalse(!!app.shadowRoot.querySelector('#error-screen'));
    assertFalse(container.hidden);
    const newIframe = container.querySelector('iframe');
    assertTrue(!!newIframe);
    assertNotEquals(oldIframe, newIframe);
    assertEquals(testUrl, newIframe.src);
  });
});
