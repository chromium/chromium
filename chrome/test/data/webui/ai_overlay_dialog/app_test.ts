// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://ai-overlay-dialog/app.js';

import type {ImageBytesResult, PageHandlerInterface, RawViewportRegionResult, RememberedNote} from 'chrome-untrusted://ai-overlay-dialog/ai_overlay_dialog.mojom-webui.js';
import {API_KEY_STORAGE_KEY, AppElement} from 'chrome-untrusted://ai-overlay-dialog/app.js';
import type {ConversationInstance} from 'chrome-untrusted://ai-overlay-dialog/app.js';
import type {ConversationConfig} from 'chrome-untrusted://ai-overlay-dialog/internal/conversation.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

function waitForShadowCondition(
    shadowRoot: ShadowRoot, check: () => boolean): Promise<void> {
  if (check()) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    const observer = new MutationObserver(() => {
      if (check()) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(
        shadowRoot, {attributes: true, childList: true, subtree: true});
  });
}

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  constructor() {
    super([
      'getMockAudioData',
      'updateAudioEnergy',
      'close',
      'getCursorPosition',
      'captureRawViewportRegion',
      'setRememberedNote',
      'getRememberedNotes',
      'getImageBytes',
      'startStreamingSession',
      'sendAudioChunk',
      'sendTextInput',
      'reportPlaybackStatus',
      'stopStreamingSession',
      'executeWebMcpTool',
    ]);
  }

  getMockAudioData(): Promise<{jsonData: string | null}> {
    this.methodCalled('getMockAudioData');
    return Promise.resolve({
      jsonData: JSON.stringify({
        buttons: [{name: 'Hello', text: 'Hello'}],
      }),
    });
  }

  updateAudioEnergy(energy: number): void {
    this.methodCalled('updateAudioEnergy', energy);
  }

  close(): void {
    this.methodCalled('close');
  }

  getCursorPosition(): ReturnType<PageHandlerInterface['getCursorPosition']> {
    this.methodCalled('getCursorPosition');
    return Promise.resolve({position: null});
  }

  captureRawViewportRegion(x: number, y: number, width: number, height: number):
      Promise<{result: RawViewportRegionResult | null}> {
    this.methodCalled('captureRawViewportRegion', {x, y, width, height});
    return Promise.resolve({result: null});
  }

  setRememberedNote(note: RememberedNote): Promise<{success: boolean}> {
    this.methodCalled('setRememberedNote', note);
    return Promise.resolve({success: true});
  }

  getRememberedNotes(): Promise<{notes: RememberedNote[]}> {
    this.methodCalled('getRememberedNotes');
    return Promise.resolve({notes: []});
  }

  getImageBytes(domNodeId: Parameters<PageHandlerInterface['getImageBytes']>[
    0
  ]): Promise<{result: ImageBytesResult | null}> {
    this.methodCalled('getImageBytes', domNodeId);
    return Promise.resolve({result: null});
  }

  startStreamingSession(): void {
    this.methodCalled('startStreamingSession');
  }

  sendAudioChunk(
      pcmData: Parameters<PageHandlerInterface['sendAudioChunk']>[0]): void {
    this.methodCalled('sendAudioChunk', pcmData);
  }

  sendTextInput(text: string): void {
    this.methodCalled('sendTextInput', text);
  }

  reportPlaybackStatus(lastPlayedSequenceNumber: bigint): void {
    this.methodCalled('reportPlaybackStatus', lastPlayedSequenceNumber);
  }

  stopStreamingSession(): void {
    this.methodCalled('stopStreamingSession');
  }

  executeWebMcpTool(
      toolName: string,
      args: Parameters<PageHandlerInterface['executeWebMcpTool']>[1]):
      ReturnType<PageHandlerInterface['executeWebMcpTool']> {
    this.methodCalled('executeWebMcpTool', {toolName, args});
    return Promise.resolve({nullValue: 0});
  }
}

class TestConversation extends TestBrowserProxy implements
    ConversationInstance {
  connected: boolean = false;
  pageContext = null;
  shouldFailStart: boolean = false;
  lastConfig: ConversationConfig|null = null;

  constructor() {
    super([
      'start',
      'stop',
      'sendAudio',
      'sendText',
      'markMockAudioEndTime',
      'recordOnDeviceSpeechTranscript',
    ]);
  }

  start(): Promise<void> {
    this.methodCalled('start', this.lastConfig);
    if (this.shouldFailStart) {
      return Promise.reject(new Error('WebSocket Closed: 1008'));
    }
    this.connected = true;
    return Promise.resolve();
  }

  stop(): void {
    this.methodCalled('stop');
    this.connected = false;
  }

  sendAudio(sampleRate: number, data: string): void {
    this.methodCalled('sendAudio', {sampleRate, data});
  }

  sendText(text: string): void {
    this.methodCalled('sendText', text);
  }

  markMockAudioEndTime(time: number): void {
    this.methodCalled('markMockAudioEndTime', time);
  }

  recordOnDeviceSpeechTranscript(text: string): void {
    this.methodCalled('recordOnDeviceSpeechTranscript', text);
  }
}

suite('AppApiKeyTest', () => {
  let pageHandler: TestPageHandler;
  let conversationProxy: TestConversation;

  function mountApp(): AppElement {
    const app = document.createElement('ai-overlay-dialog-app');
    document.body.appendChild(app);
    return app;
  }

  setup(() => {
    document.body.innerHTML =
        window.trustedTypes ? window.trustedTypes.emptyHTML : '';
    window.localStorage.removeItem(API_KEY_STORAGE_KEY);
    pageHandler = new TestPageHandler();
    conversationProxy = new TestConversation();
    AppElement.setPageHandlerForTesting(pageHandler);
    AppElement.setConversationFactoryForTesting(
        (config: ConversationConfig) => {
          conversationProxy.lastConfig = config;
          return conversationProxy;
        });
  });

  teardown(() => {
    document.body.innerHTML =
        window.trustedTypes ? window.trustedTypes.emptyHTML : '';
    window.localStorage.removeItem(API_KEY_STORAGE_KEY);
    AppElement.setPageHandlerForTesting(null);
    AppElement.setConversationFactoryForTesting(null);
    loadTimeData.resetForTesting();
  });

  test('PromptsForApiKeyWhenNoneConfigured', async () => {
    loadTimeData.resetForTesting({
      apiKey: '',
      ttcBundleUrl: '',
      allowBuiltInDefaults: true,
      useMes: false,
      isAndroidBackend: true,
    });

    const app = mountApp();
    await waitForShadowCondition(
        app.shadowRoot,
        () => app.shadowRoot.querySelector('#api-key-prompt') !== null);

    const input =
        app.shadowRoot?.querySelector<HTMLInputElement>('#api-key-input');
    const saveButton = app.shadowRoot?.querySelector<HTMLButtonElement>(
        '#api-key-save-button');
    assertTrue(Boolean(input));
    assertTrue(Boolean(saveButton));
    assertEquals('password', input!.type);
    assertEquals('Enter Gemini API key', input!.placeholder);
    assertEquals('off', input!.autocomplete);
    assertNull(app.shadowRoot?.querySelector('#api-key-error') ?? null);
    assertEquals(0, conversationProxy.getCallCount('start'));
  });

  test('SavingApiKeyPersistsAndStartsConversation', async () => {
    loadTimeData.resetForTesting({
      apiKey: '',
      ttcBundleUrl: '',
      allowBuiltInDefaults: true,
      useMes: false,
      isAndroidBackend: true,
    });

    const app = mountApp();
    await waitForShadowCondition(
        app.shadowRoot,
        () => app.shadowRoot.querySelector('#api-key-prompt') !== null);

    const input =
        app.shadowRoot.querySelector<HTMLInputElement>('#api-key-input')!;
    const saveButton = app.shadowRoot.querySelector<HTMLButtonElement>(
        '#api-key-save-button')!;

    input.value = '  test-saved-key-123  ';
    input.dispatchEvent(new Event('input'));
    saveButton.click();

    const startedConfig =
        await conversationProxy.whenCalled('start') as ConversationConfig;
    await microtasksFinished();

    assertEquals(
        'test-saved-key-123', window.localStorage.getItem(API_KEY_STORAGE_KEY));
    assertEquals('test-saved-key-123', startedConfig.api_config.apiKey);
    assertNull(app.shadowRoot?.querySelector('#api-key-prompt') ?? null);

    // Remounting a fresh AppElement should reuse localStorage without
    // prompting.
    app.remove();
    conversationProxy.resetResolver('start');

    const remountedApp = mountApp();
    const secondConfig =
        await conversationProxy.whenCalled('start') as ConversationConfig;
    await microtasksFinished();

    assertEquals('test-saved-key-123', secondConfig.api_config.apiKey);
    assertNull(
        remountedApp.shadowRoot?.querySelector('#api-key-prompt') ?? null);
  });

  test('EnterKeySubmitsApiKey', async () => {
    loadTimeData.resetForTesting({
      apiKey: '',
      ttcBundleUrl: '',
      allowBuiltInDefaults: true,
      useMes: false,
      isAndroidBackend: true,
    });

    const app = mountApp();
    await waitForShadowCondition(
        app.shadowRoot,
        () => app.shadowRoot.querySelector('#api-key-prompt') !== null);

    const input =
        app.shadowRoot.querySelector<HTMLInputElement>('#api-key-input')!;
    input.value = 'enter-key-456';
    input.dispatchEvent(new Event('input'));
    input.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));

    const startedConfig =
        await conversationProxy.whenCalled('start') as ConversationConfig;
    await microtasksFinished();

    assertEquals(
        'enter-key-456', window.localStorage.getItem(API_KEY_STORAGE_KEY));
    assertEquals('enter-key-456', startedConfig.api_config.apiKey);
  });

  test('CliApiKeyBypassesPromptAndOverridesSavedKey', async () => {
    window.localStorage.setItem(API_KEY_STORAGE_KEY, 'saved-fallback-key');
    loadTimeData.resetForTesting({
      apiKey: 'cli-flag-key-789',
      ttcBundleUrl: '',
      allowBuiltInDefaults: true,
      useMes: false,
      isAndroidBackend: true,
    });

    const app = mountApp();
    const startedConfig =
        await conversationProxy.whenCalled('start') as ConversationConfig;
    await microtasksFinished();

    assertEquals('cli-flag-key-789', startedConfig.api_config.apiKey);
    assertNull(app.shadowRoot?.querySelector('#api-key-prompt') ?? null);
  });

  test('ChangeApiKeyRecoversFromFailedConnection', async () => {
    window.localStorage.setItem(API_KEY_STORAGE_KEY, 'expired-or-typo-key');
    conversationProxy.shouldFailStart = true;
    loadTimeData.resetForTesting({
      apiKey: '',
      ttcBundleUrl: '',
      allowBuiltInDefaults: true,
      useMes: false,
      isAndroidBackend: true,
    });

    const app = mountApp();
    await conversationProxy.whenCalled('start');
    await waitForShadowCondition(
        app.shadowRoot,
        () => app.shadowRoot.querySelector('#api-key-error') !== null);

    const changeButton = app.shadowRoot.querySelector<HTMLButtonElement>(
        '#change-api-key-button');
    assertTrue(Boolean(changeButton));

    changeButton!.click();
    await microtasksFinished();

    assertNull(window.localStorage.getItem(API_KEY_STORAGE_KEY));
    assertTrue(app.shadowRoot?.querySelector('#api-key-prompt') !== null);

    conversationProxy.resetResolver('start');
    conversationProxy.shouldFailStart = false;

    const input =
        app.shadowRoot.querySelector<HTMLInputElement>('#api-key-input')!;
    input.value = 'fresh-working-key';
    input.dispatchEvent(new Event('input'));
    app.shadowRoot.querySelector<HTMLButtonElement>(
                      '#api-key-save-button')!.click();

    const retryConfig =
        await conversationProxy.whenCalled('start') as ConversationConfig;
    await microtasksFinished();

    assertEquals('fresh-working-key', retryConfig.api_config.apiKey);
    assertNull(app.shadowRoot?.querySelector('#api-key-error') ?? null);
    assertNull(app.shadowRoot?.querySelector('#api-key-prompt') ?? null);
  });

  test('CustomBundleApiKeyFallbackAndSavedKeyPrecedence', async () => {
    const originalFetch = window.fetch;
    window.fetch = (input: RequestInfo|URL) => {
      const url = String(input);
      if (url.endsWith('persona.json')) {
        return Promise.resolve(new Response(JSON.stringify({
          personas: [{
            id: 'dev',
            name: 'Dev',
            nicknames: [],
            persona: '',
            voice: 'Achernar',
          }],
        })));
      }
      if (url.endsWith('api_config.json')) {
        return Promise.resolve(new Response(JSON.stringify({
          endpointUrl: 'wss://example.test/ws',
          model: 'models/test',
          apiKey: 'bundle-supplied-key',
        })));
      }
      if (url.endsWith('instruction.tmpl')) {
        return Promise.resolve(new Response('System instruction'));
      }
      return Promise.resolve(
          new Response(new Blob([''], {type: 'video/webm'})));
    };

    try {
      // 1) With no CLI or saved key, custom bundle's apiKey is used (#3).
      loadTimeData.resetForTesting({
        apiKey: '',
        ttcBundleUrl: 'https://example.test/bundle/',
        allowBuiltInDefaults: true,
        useMes: false,
        isAndroidBackend: true,
      });

      const app = mountApp();
      const bundleConfig =
          await conversationProxy.whenCalled('start') as ConversationConfig;
      await microtasksFinished();

      assertEquals('bundle-supplied-key', bundleConfig.api_config.apiKey);
      assertNull(app.shadowRoot?.querySelector('#api-key-prompt') ?? null);

      // 2) With a saved key in localStorage, saved key overrides bundle (#2).
      app.remove();
      conversationProxy.resetResolver('start');
      window.localStorage.setItem(API_KEY_STORAGE_KEY, 'user-saved-override');

      const secondApp = mountApp();
      const savedOverrideConfig =
          await conversationProxy.whenCalled('start') as ConversationConfig;
      await microtasksFinished();

      assertEquals(
          'user-saved-override', savedOverrideConfig.api_config.apiKey);
      assertNull(
          secondApp.shadowRoot?.querySelector('#api-key-prompt') ?? null);
    } finally {
      window.fetch = originalFetch;
    }
  });
});
