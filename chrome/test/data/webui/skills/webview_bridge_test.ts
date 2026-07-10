// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {ToastType} from 'chrome://skills/skills.mojom-webui.js';
import type {SkillsWebviewBridgeDelegate} from 'chrome://skills/v2/skills_webview_bridge.js';
import {SkillsWebviewBridge} from 'chrome://skills/v2/skills_webview_bridge.js';
import {HANDSHAKE_TIMEOUT_MS, SKILLS_HANDSHAKE_ACK, SKILLS_HANDSHAKE_TYPE, SKILLS_HOST_URL, SKILLS_SHOW_TOAST} from 'chrome://skills/v2/skills_webview_bridge_constants.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

import {createSkillsHostProxyOnLoad} from './api_boot.js';


suite('SkillsWebviewBridgeTest', () => {
  let bridge: SkillsWebviewBridge;
  let webview: chrome.webviewTag.WebView;
  let postedMessages: Array<{type?: string}> = [];
  let originalPostMessage: Function;
  let onPostMessage: (message: {type?: string}) => void;

  setup(() => {
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {};
    }
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
    });
    postedMessages = [];
    onPostMessage = () => {};
    originalPostMessage = window.postMessage;

    Object.defineProperty(window, 'postMessage', {
      value: function(
          message: {type?: string}, targetOrigin: string,
          transfer?: unknown[]) {
        if (message) {
          postedMessages.push(message);
          onPostMessage(message);
        }
        return originalPostMessage.call(
            window, message, targetOrigin, transfer);
      },
      configurable: true,
    });

    // We must use a div element mocked as a WebView here. An actual WebView
    // element's `contentWindow` property is non-configurable and cannot be
    // redefined via Object.defineProperty.
    const div = document.createElement('div');
    Object.assign(div, {
      contentWindow: window,
    });
    webview = div as unknown as chrome.webviewTag.WebView;
  });

  teardown(() => {
    onPostMessage = () => {};
    if (originalPostMessage) {
      Object.defineProperty(window, 'postMessage', {
        value: originalPostMessage,
        configurable: true,
      });
    }
    if (bridge) {
      bridge.destroy();
    }
  });

  test('HostInitiatesHandshakeAndReceivesAck', async () => {
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    assertFalse(bridge.isConnected());

    // Set up a promise to resolve when the handshake ping is posted.
    const pingPromise = new Promise<{type?: string}>(resolve => {
      onPostMessage = (message) => {
        if (message.type === SKILLS_HANDSHAKE_TYPE) {
          resolve(message);
        }
      };
    });

    // Trigger loadcommit to start handshake.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(event, 'url', {value: SKILLS_HOST_URL});
    webview.dispatchEvent(event);

    // Verify ping was sent.
    await pingPromise;
    assertTrue(postedMessages.length > 0);
    assertEquals(SKILLS_HANDSHAKE_TYPE, postedMessages[0]!.type);

    // Set up a promise to resolve when the handshake ACK is processed.
    const ackPromise = new Promise<void>(resolve => {
      const handler = (e: MessageEvent) => {
        if (e.data && e.data.type === SKILLS_HANDSHAKE_ACK) {
          resolve();
        }
      };
      window.addEventListener('message', handler);
    });

    // Send matching ACK via mock MessageEvent to simulate correct origin.
    const messageEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: new URL(SKILLS_HOST_URL).origin,
      source: window,
    });
    window.dispatchEvent(messageEvent);

    await ackPromise;
    assertTrue(bridge.isConnected());
  });

  test('GuestApiBootPerformsHandshake', async () => {
    const bootPromise = createSkillsHostProxyOnLoad(window.location.origin);

    // Send Chrome ping to guest boot listener via real window.postMessage.
    window.postMessage(
        {
          type: SKILLS_HANDSHAKE_TYPE,
        },
        '*');

    await bootPromise;

    // Verify ACK was sent to Chrome host.
    const ackMessage =
        postedMessages.find(m => m.type === SKILLS_HANDSHAKE_ACK);
    assertTrue(!!ackMessage);
  });

  test('HandshakeTimesOut', () => {
    let errorCalled = false;
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {
        errorCalled = true;
      },
      onShowToast: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    const mockTimer = new MockTimer();
    mockTimer.install();

    // Trigger loadcommit to start handshake.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(event, 'url', {value: SKILLS_HOST_URL});
    webview.dispatchEvent(event);

    // The error callback should not be called immediately.
    assertFalse(errorCalled);

    // Fast-forward time to trigger handshake timeout.
    mockTimer.tick(HANDSHAKE_TIMEOUT_MS);

    assertTrue(errorCalled);
    assertFalse(bridge.isConnected());

    mockTimer.uninstall();
  });

  test('HostReceivesShowToastMessage_Delete', () => {
    let receivedToastType: ToastType|null = null;
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: (toastType: ToastType) => {
        receivedToastType = toastType;
      },
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit to start handshake.
    const loadEvent = new CustomEvent('loadcommit');
    Object.defineProperty(loadEvent, 'isTopLevel', {value: true});
    Object.defineProperty(loadEvent, 'url', {value: SKILLS_HOST_URL});
    webview.dispatchEvent(loadEvent);

    // Send mock ACK to complete handshake.
    const ackEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: new URL(SKILLS_HOST_URL).origin,
      source: window,
    });
    window.dispatchEvent(ackEvent);

    assertTrue(bridge.isConnected());

    // Send show-toast message via mock MessageEvent to match origin.
    const toastEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_SHOW_TOAST,
        toastType: 'delete',
      },
      origin: new URL(SKILLS_HOST_URL).origin,
      source: window,
    });
    window.dispatchEvent(toastEvent);

    assertEquals(ToastType.kDelete, receivedToastType);
  });
});
