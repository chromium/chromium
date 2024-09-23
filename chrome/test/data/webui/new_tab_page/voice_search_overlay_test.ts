// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {VoiceSearchOverlayElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, NewTabPageProxy, VoiceAction as Action, VoiceError as Error, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, installMock, keydown} from './test_support.js';

function createResults(n: number): SpeechRecognitionEvent {
  return {
    results: Array.from(Array(n)).map(() => {
      return {
        isFinal: false,
        0: {
          transcript: 'foo',
          confidence: 1,
        },
      } as unknown as SpeechRecognitionResult;
    }),
    resultIndex: 0,
  } as unknown as SpeechRecognitionEvent;
}

// eslint-disable-next-line @typescript-eslint/naming-convention
type webkitSpeechRecognitionError = SpeechRecognitionErrorEvent;
declare const webkitSpeechRecognitionError: typeof SpeechRecognitionErrorEvent;

class MockSpeechRecognition {
  startCalled: boolean = false;
  abortCalled: boolean = false;

  continuous: boolean = false;
  interimResults: boolean = true;
  lang: string = window.navigator.language;

  onaudiostart: (() => void)|null = null;
  onspeechstart: (() => void)|null = null;
  onresult:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionEvent) => void)|null = null;
  onend: (() => void)|null = null;
  onerror:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionErrorEvent) => void)|null = null;
  onnomatch: (() => void)|null = null;

  constructor() {
    mockSpeechRecognition = this;
  }

  start() {
    this.startCalled = true;
  }

  abort() {
    this.abortCalled = true;
  }
}

let mockSpeechRecognition: MockSpeechRecognition;

suite('NewTabPageVoiceSearchOverlayTest', () => {
  let voiceSearchOverlay: VoiceSearchOverlayElement;
  let windowProxy: TestMock<WindowProxy>;
  let metrics: MetricsTracker;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));

    metrics = fakeMetricsPrivate();

    voiceSearchOverlay = document.createElement('ntp-voice-search-overlay');
    document.body.appendChild(voiceSearchOverlay);
    await flushTasks();
  });

  test('creating overlay opens native dialog', () => {
    // Assert.
    assertTrue(voiceSearchOverlay.$.dialog.open);
  });

  test('creating overlay starts speech recognition', () => {
    // Assert.
    assertTrue(mockSpeechRecognition.startCalled);
  });

  test('creating overlay shows waiting text', () => {
    // Assert.
    assertTrue(isVisible(voiceSearchOverlay.shadowRoot!.querySelector(
        '#texts *[text=waiting]')));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on audio received shows speak text', async () => {
    // Act.
    mockSpeechRecognition.onaudiostart!();
    await microtasksFinished();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot!.querySelector('#texts *[text=speak]')));
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on speech received starts volume animation', async () => {
    // Arrange.
    windowProxy.setResultFor('random', 0.5);

    // Act.
    mockSpeechRecognition.onspeechstart!();
    await microtasksFinished();

    // Assert.
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0.5');
  });

  test('on result received shows recognized text', async () => {
    // Arrange.
    windowProxy.setResultFor('random', 0.5);
    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 0, transcript: 'world'});

    // Act.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    // Assert.
    const [intermediateResult, finalResult] =
        voiceSearchOverlay.shadowRoot!.querySelectorAll<HTMLElement>(
            '#texts *[text=result] span');
    assertTrue(isVisible(intermediateResult!));
    assertTrue(isVisible(finalResult!));
    assertEquals(intermediateResult!.innerText, 'hello');
    assertEquals(finalResult!.innerText, 'world');
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0.5');
  });

  test('on final result received queries google', async () => {
    // Arrange.
    const googleBaseUrl = 'https://google.com';
    loadTimeData.overrideValues({googleBaseUrl: googleBaseUrl});
    windowProxy.setResultFor('random', 0);
    const result = createResults(1);
    Object.assign(result.results[0]!, {isFinal: true});
    Object.assign(result.results[0]![0]!, {transcript: 'hello world'});

    // Act.
    mockSpeechRecognition.onresult!(result);

    // Assert.
    const href = await windowProxy.whenCalled('navigate');
    assertEquals(href, `${googleBaseUrl}/search?q=hello+world&gs_ivs=1`);
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', Action.QUERY_SUBMITTED));
  });

  ([
    ['no-speech', 'no-speech', 'learn-more', Error.NO_SPEECH],
    ['audio-capture', 'audio-capture', 'learn-more', Error.AUDIO_CAPTURE],
    ['network', 'network', 'none', Error.NETWORK],
    ['not-allowed', 'not-allowed', 'details', Error.NOT_ALLOWED],
    [
      'service-not-allowed',
      'not-allowed',
      'details',
      Error.SERVICE_NOT_ALLOWED,
    ],
    [
      'language-not-supported',
      'language-not-supported',
      'none',
      Error.LANGUAGE_NOT_SUPPORTED,
    ],
    ['aborted', 'other', 'none', Error.ABORTED],
    ['bad-grammar', 'other', 'none', Error.BAD_GRAMMAR],
    ['foo', 'other', 'none', Error.OTHER],
    ['no-match', 'no-match', 'try-again', Error.NO_MATCH],
  ] as Array<[SpeechRecognitionErrorCode & 'no-match', string, string, Error]>)
      .forEach(([error, text, link, logError]) => {
        test(`on '${error}' received shows error text`, async () => {
          // Act.
          if (error === 'no-match') {
            mockSpeechRecognition.onnomatch!();
          } else {
            mockSpeechRecognition.onerror!
                (new webkitSpeechRecognitionError('error', {error}));
          }
          await microtasksFinished();

          // Assert.
          assertTrue(isVisible(voiceSearchOverlay.shadowRoot!.querySelector(
              '#texts *[text=error]')));
          assertTrue(isVisible(voiceSearchOverlay.shadowRoot!.querySelector(
              `#errors *[error="${text}"]`)));
          assertNotStyle(
              voiceSearchOverlay.shadowRoot!.querySelector(
                  `#errorLinks *[link="${link}"]`)!,
              'display', 'none');
          assertFalse(voiceSearchOverlay.$.micContainer.classList.contains(
              'listening'));
          assertFalse(voiceSearchOverlay.$.micContainer.classList.contains(
              'receiving'));
          assertStyle(
              voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
          assertEquals(1, metrics.count('NewTabPage.VoiceErrors'));
          assertEquals(1, metrics.count('NewTabPage.VoiceErrors', logError));
        });
      });

  test('on end received shows error text if no final result', async () => {
    // Act.
    mockSpeechRecognition.onend!();
    await microtasksFinished();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot!.querySelector('#texts *[text=error]')));
    assertTrue(isVisible(voiceSearchOverlay.shadowRoot!.querySelector(
        '#errors *[error="audio-capture"]')));
  });

  test('on end received shows result text if final result', async () => {
    // Arrange.
    const result = createResults(1);
    Object.assign(result.results[0]!, {isFinal: true});

    // Act.
    mockSpeechRecognition.onresult!(result);
    mockSpeechRecognition.onend!();
    await microtasksFinished();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot!.querySelector('#texts *[text=result]')));
  });

  const testParams = [
    {
      functionName: 'onaudiostart',
      arguments: [],
    },
    {
      functionName: 'onspeechstart',
      arguments: [],
    },
    {
      functionName: 'onresult',
      arguments: [createResults(1)],
    },
    {
      functionName: 'onend',
      arguments: [],
    },
    {
      functionName: 'onerror',
      arguments: [{error: 'audio-capture'}],
    },
    {
      functionName: 'onnomatch',
      arguments: [],
    },
  ];

  testParams.forEach(function(param) {
    test(`${param.functionName} received resets timer`, async () => {
      // Act.
      // Need to account for previously set timers.
      windowProxy.resetResolver('setTimeout');
      (mockSpeechRecognition as any)[param.functionName](...param.arguments);

      // Assert.
      await windowProxy.whenCalled('setTimeout');
    });
  });

  test('on idle timeout shows error text', async () => {
    // Arrange.
    const [callback, _] = await windowProxy.whenCalled('setTimeout');

    // Act.
    callback();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot!.querySelector('#texts *[text=error]')));
  });

  test('on error timeout closes overlay', async () => {
    // Arrange.
    // Need to account for previously set idle timer.
    windowProxy.resetResolver('setTimeout');
    mockSpeechRecognition.onerror!
        (new webkitSpeechRecognitionError('error', {error: 'audio-capture'}));

    // Act.
    const [callback, _] = await windowProxy.whenCalled('setTimeout');
    callback();

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
  });

  test('on idle timeout stops voice search', async () => {
    // Arrange.
    const [callback, _] = await windowProxy.whenCalled('setTimeout');

    // Act.
    callback();

    // Assert.
    assertTrue(mockSpeechRecognition.abortCalled);
  });

  test(`clicking '#retryLink' starts voice search if in retry state`, () => {
    // Arrange.
    mockSpeechRecognition.onnomatch!();
    mockSpeechRecognition.startCalled = false;

    // Act.
    $$<HTMLElement>(voiceSearchOverlay, '#retryLink')!.click();

    // Assert.
    assertTrue(mockSpeechRecognition.startCalled);
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', Action.TRY_AGAIN_LINK));
  });

  [' ', 'Enter'].forEach(key => {
    test(`'${key}' submits query if result`, () => {
      // Arrange.
      mockSpeechRecognition.onresult!(createResults(1));
      assertEquals(0, windowProxy.getCallCount('navigate'));

      // Act.
      keydown(voiceSearchOverlay.shadowRoot!.activeElement as HTMLElement, key);

      // Assert.
      assertEquals(1, windowProxy.getCallCount('navigate'));
      assertTrue(voiceSearchOverlay.$.dialog.open);
    });

    test(`'${key}' does not submit query if no result`, () => {
      // Act.
      keydown(voiceSearchOverlay.shadowRoot!.activeElement as HTMLElement, key);

      // Assert.
      assertEquals(0, windowProxy.getCallCount('navigate'));
      assertTrue(voiceSearchOverlay.$.dialog.open);
    });

    test(`'${key}' triggers link`, () => {
      // Arrange.
      mockSpeechRecognition.onerror!
          (new webkitSpeechRecognitionError('error', {error: 'audio-capture'}));
      const link =
          $$<HTMLAnchorElement>(voiceSearchOverlay, '[link=learn-more]')!;
      link.href = '#';
      link.target = '_self';
      let clicked = false;
      link.addEventListener('click', () => clicked = true);

      // Act.
      keydown(link, key);

      // Assert.
      assertTrue(clicked);
      assertEquals(0, windowProxy.getCallCount('navigate'));
      assertFalse(voiceSearchOverlay.$.dialog.open);
    });
  });

  test('\'Escape\' closes overlay', () => {
    // Act.
    keydown(
        voiceSearchOverlay.shadowRoot!.activeElement as HTMLElement, 'Escape');

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', Action.CLOSE_OVERLAY));
  });

  test('Click closes overlay', () => {
    // Act.
    voiceSearchOverlay.$.dialog.click();

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', Action.CLOSE_OVERLAY));
  });

  test('Clicking learn more logs action', () => {
    // Arrange.
    mockSpeechRecognition.onerror!
        (new webkitSpeechRecognitionError('error', {error: 'audio-capture'}));
    const link =
        $$<HTMLAnchorElement>(voiceSearchOverlay, '[link=learn-more]')!;
    link.href = '#';
    link.target = '_self';

    // Act.
    link.click();

    // Assert.
    assertEquals(
        1,
        metrics.count('NewTabPage.VoiceActions', Action.SUPPORT_LINK_CLICKED));
  });
});
