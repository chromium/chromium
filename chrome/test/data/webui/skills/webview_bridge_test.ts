// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {ToastType} from 'chrome://skills/skills.mojom-webui.js';
import {SkillsWebview} from 'chrome://skills/v2/skills_webview.js';
import type {SkillsWebviewBridgeDelegate} from 'chrome://skills/v2/skills_webview_bridge.js';
import {SkillsWebviewBridge} from 'chrome://skills/v2/skills_webview_bridge.js';
import {getChromePathForRemoteUrl, getLoadingStageHistogramName, getPrimarySkillsOrigin, getRemoteUrlForChromePath, getSkillsRemoteUrl, HANDSHAKE_TIMEOUT_MS, HISTOGRAM_HANDSHAKE_RESULT, LoadingStage, SKILLS_HANDSHAKE_ACK, SKILLS_HANDSHAKE_TYPE, SKILLS_INVOKE_SKILL, SKILLS_LOG_METRIC, SKILLS_OPEN_URL, SKILLS_SHOW_TOAST} from 'chrome://skills/v2/skills_webview_bridge_constants.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

import {createSkillsHostProxyOnLoad} from './api_boot.js';


interface RecordedHistogram {
  name: string;
  value: number|boolean;
  type: string;
}

suite('SkillsWebviewBridgeTest', () => {
  let bridge: SkillsWebviewBridge;
  let webview: chrome.webviewTag.WebView;
  let postedMessages: Array<{type?: string}> = [];
  let originalPostMessage: Function;
  let onPostMessage: (message: {type?: string}) => void;
  let recordedHistograms: RecordedHistogram[] = [];

  setup(() => {
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {};
    }
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      skillsPrimaryOrigin: 'https://clients5.google.com',
    });
    postedMessages = [];
    recordedHistograms = [];
    onPostMessage = () => {};
    originalPostMessage = window.postMessage;

    interface WindowWithChrome extends Window {
      chrome?: {
        histograms?: {
          recordMediumTime: (name: string, value: number) => void,
          recordBoolean: (name: string, value: boolean) => void,
        },
      };
    }
    const windowWithChrome = window as unknown as WindowWithChrome;
    windowWithChrome.chrome = windowWithChrome.chrome || {};
    windowWithChrome.chrome.histograms = {
      recordMediumTime: (name: string, value: number) => {
        recordedHistograms.push({name, value, type: 'medium-time'});
      },
      recordBoolean: (name: string, value: boolean) => {
        recordedHistograms.push({name, value, type: 'boolean'});
      },
    };

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
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
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
    Object.defineProperty(event, 'url', {value: getSkillsRemoteUrl()});
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
      origin: getPrimarySkillsOrigin(),
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
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    const mockTimer = new MockTimer();
    mockTimer.install();

    // Trigger loadcommit to start handshake.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(event, 'url', {value: getSkillsRemoteUrl()});
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
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit to start handshake.
    const loadEvent = new CustomEvent('loadcommit');
    Object.defineProperty(loadEvent, 'isTopLevel', {value: true});
    Object.defineProperty(loadEvent, 'url', {value: getSkillsRemoteUrl()});
    webview.dispatchEvent(loadEvent);

    // Send mock ACK to complete handshake.
    const ackEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: getPrimarySkillsOrigin(),
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
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(toastEvent);

    assertEquals(ToastType.kDelete, receivedToastType);
  });

  test('HostReceivesInvokeSkillMessage', () => {
    let receivedSkillId: string|null = null;
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
      onInvokeSkill: (skillId: string) => {
        receivedSkillId = skillId;
      },
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit to start handshake.
    const loadEvent = new CustomEvent('loadcommit');
    Object.defineProperty(loadEvent, 'isTopLevel', {value: true});
    Object.defineProperty(loadEvent, 'url', {value: getSkillsRemoteUrl()});
    webview.dispatchEvent(loadEvent);

    // Send mock ACK to complete handshake.
    const ackEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(ackEvent);

    assertTrue(bridge.isConnected());

    // Send invoke-skill message via mock MessageEvent to match origin.
    const invokeEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_INVOKE_SKILL,
        skillId: 'some_skill_id',
      },
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(invokeEvent);

    assertEquals('some_skill_id', receivedSkillId);
  });

  test('HostReceivesUrlChangedEvent', () => {
    const received = {url: null as URL | null};
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
      onInvokeSkill: () => {},
      onUrlChanged: (url: URL) => {
        received.url = url;
      },
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit with specific URL.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(
        event, 'url', {value: getRemoteUrlForChromePath('/yourSkills')});
    webview.dispatchEvent(event);

    assertEquals(
        getRemoteUrlForChromePath('/yourSkills'), received.url?.href ?? '');
  });

  test('GetChromePathForRemoteUrl_ValidPath', () => {
    const url = new URL(getRemoteUrlForChromePath('/yourSkills'));
    assertEquals('/yourSkills', getChromePathForRemoteUrl(url));
  });

  test('GetChromePathForRemoteUrl_InvalidOriginDefaultsToBrowse', () => {
    const url = new URL('https://invalidorigin.com/chromeskills/yourSkills');
    assertEquals('/browse', getChromePathForRemoteUrl(url));
  });

  test('GetChromePathForRemoteUrl_InvalidPathDefaultsToBrowse', () => {
    const url = new URL(`${getPrimarySkillsOrigin()}/invalidpath/yourSkills`);
    assertEquals('/browse', getChromePathForRemoteUrl(url));
  });
  test('HandshakeLogsMetricsOnSuccess', () => {
    let handshakeCompleteCalled = false;
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {
        handshakeCompleteCalled = true;
      },
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit to start handshake.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(event, 'url', {value: getSkillsRemoteUrl()});
    webview.dispatchEvent(event);

    // Send matching ACK to simulate success.
    const messageEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(messageEvent);

    assertTrue(bridge.isConnected());
    assertTrue(handshakeCompleteCalled);

    // Verify histograms
    const handshakeMetric = recordedHistograms.find(
        h => h.name === getLoadingStageHistogramName(LoadingStage.HANDSHAKE));
    assertTrue(!!handshakeMetric);
    assertEquals('medium-time', handshakeMetric.type);
    assertTrue((handshakeMetric.value as number) >= 0);

    const resultMetric =
        recordedHistograms.find(h => h.name === HISTOGRAM_HANDSHAKE_RESULT);
    assertTrue(!!resultMetric);
    assertEquals('boolean', resultMetric.type);
    assertTrue(resultMetric.value as boolean);
  });

  test('HandshakeLogsMetricsOnTimeout', () => {
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    const mockTimer = new MockTimer();
    mockTimer.install();

    // Trigger loadcommit to start handshake.
    const event = new CustomEvent('loadcommit');
    Object.defineProperty(event, 'isTopLevel', {value: true});
    Object.defineProperty(event, 'url', {value: getSkillsRemoteUrl()});
    webview.dispatchEvent(event);

    // Fast-forward time to trigger handshake timeout.
    mockTimer.tick(HANDSHAKE_TIMEOUT_MS);

    assertFalse(bridge.isConnected());

    const resultMetric =
        recordedHistograms.find(h => h.name === HISTOGRAM_HANDSHAKE_RESULT);
    assertTrue(!!resultMetric);
    assertEquals('boolean', resultMetric.type);
    assertFalse(resultMetric.value as boolean);

    mockTimer.uninstall();
  });

  test('HostLogsGuestMetrics', () => {
    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => {},
      onShowToast: () => {},
      onInvokeSkill: () => {},
      onUrlChanged: () => {},
      onCloseDialog: () => {},
      onHandshakeComplete: () => {},
    };
    bridge = new SkillsWebviewBridge(webview, delegate);

    // Trigger loadcommit to start handshake.
    const loadEvent = new CustomEvent('loadcommit');
    Object.defineProperty(loadEvent, 'isTopLevel', {value: true});
    Object.defineProperty(loadEvent, 'url', {value: getSkillsRemoteUrl()});
    webview.dispatchEvent(loadEvent);

    // Send mock ACK to complete handshake.
    const ackEvent = new MessageEvent('message', {
      data: {type: SKILLS_HANDSHAKE_ACK},
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(ackEvent);

    assertTrue(bridge.isConnected());

    // Send guest metric 'framework-load-time'
    const frameworkEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_LOG_METRIC,
        metricName: 'framework-load-time',
        valueMs: 123,
      },
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(frameworkEvent);

    // Send guest metric 'web-client-load-time'
    const webClientEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_LOG_METRIC,
        metricName: 'web-client-load-time',
        valueMs: 456,
      },
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(webClientEvent);

    // Send guest metric 'guest-data-fetch-time' (as float)
    const dataFetchEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_LOG_METRIC,
        metricName: 'guest-data-fetch-time',
        valueMs: 789.6,
      },
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(dataFetchEvent);

    // Send guest metric 'guest-data-save-time' (as float)
    const dataSaveEvent = new MessageEvent('message', {
      data: {
        type: SKILLS_LOG_METRIC,
        metricName: 'guest-data-save-time',
        valueMs: 1011.2,
      },
      origin: getPrimarySkillsOrigin(),
      source: window,
    });
    window.dispatchEvent(dataSaveEvent);

    // Verify histograms
    const frameworkMetric = recordedHistograms.find(
        h => h.name ===
            getLoadingStageHistogramName(LoadingStage.GUEST_FRAMEWORK));
    assertTrue(!!frameworkMetric);
    assertEquals(123, frameworkMetric.value);

    const webClientMetric = recordedHistograms.find(
        h => h.name ===
            getLoadingStageHistogramName(LoadingStage.GUEST_WEB_CLIENT));
    assertTrue(!!webClientMetric);
    assertEquals(456, webClientMetric.value);

    const dataFetchMetric = recordedHistograms.find(
        h => h.name === 'Skills.Webview.LoadingStageDuration.GUEST_DATA_FETCH');
    assertTrue(!!dataFetchMetric);
    assertEquals(789, dataFetchMetric.value);

    const dataSaveMetric =
        recordedHistograms.find(h => h.name === 'Skills.Webview.WriteLatency');
    assertTrue(!!dataSaveMetric);
    assertEquals(1011, dataSaveMetric.value);
  });

  test('SkillsWebview_GetInitStartTime', () => {
    const webviewApp = new SkillsWebview();

    // Test with openStartTime
    const timeOrigin = performance.timeOrigin;
    const openStartTimeMs = timeOrigin + 5000;
    const params = new URLSearchParams(`?openStartTime=${openStartTimeMs}`);

    const startTime = webviewApp.getInitStartTimeForTesting(params);
    assertEquals(5000, startTime);

    // Test without openStartTime
    const paramsEmpty = new URLSearchParams('');
    const startTimeEmpty = webviewApp.getInitStartTimeForTesting(paramsEmpty);
    const now = performance.now();
    assertTrue(Math.abs(startTimeEmpty - now) < 50);
  });

  test('HostReceivesOpenUrlMessage', () => {
    let openedUrl: string|null = null;
    let openedTarget: string|null = null;
    const originalOpen = window.open;
    window.open = (url?: string|URL, target?: string) => {
      openedUrl = url ? url.toString() : null;
      openedTarget = target || null;
      return null;
    };

    try {
      const delegate: SkillsWebviewBridgeDelegate = {
        onError: () => {},
        onShowToast: () => {},
        onInvokeSkill: () => {},
        onUrlChanged: () => {},
        onCloseDialog: () => {},
        onHandshakeComplete: () => {},
      };
      bridge = new SkillsWebviewBridge(webview, delegate);

      // Trigger loadcommit to start handshake.
      const loadEvent = new CustomEvent('loadcommit');
      Object.defineProperty(loadEvent, 'isTopLevel', {value: true});
      Object.defineProperty(loadEvent, 'url', {value: getSkillsRemoteUrl()});
      webview.dispatchEvent(loadEvent);

      // Send mock ACK to complete handshake.
      const ackEvent = new MessageEvent('message', {
        data: {type: SKILLS_HANDSHAKE_ACK},
        origin: getPrimarySkillsOrigin(),
        source: window,
      });
      window.dispatchEvent(ackEvent);

      assertTrue(bridge.isConnected());

      // Send open-url message via mock MessageEvent to match origin.
      const openUrlEvent = new MessageEvent('message', {
        data: {
          type: SKILLS_OPEN_URL,
          url: 'https://example.com/foo',
        },
        origin: getPrimarySkillsOrigin(),
        source: window,
      });
      window.dispatchEvent(openUrlEvent);

      assertEquals('https://example.com/foo', openedUrl);
      assertEquals('_blank', openedTarget);
    } finally {
      window.open = originalOpen;
    }
  });
});
