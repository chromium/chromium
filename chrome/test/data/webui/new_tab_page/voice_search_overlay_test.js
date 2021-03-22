// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {$$, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {flushTasks, isVisible} from 'chrome://test/test_util.m.js';

import {assertNotStyle, assertStyle, keydown} from './test_support.js';

/** @typedef {newTabPage.mojom.VoiceSearchAction} */
const Action = newTabPage.mojom.VoiceSearchAction;

/** @typedef {newTabPage.mojom.VoiceSearchError} */
const Error = newTabPage.mojom.VoiceSearchError;

function createResults(n) {
  return {
    results: Array.from(Array(n)).map(() => {
      return {
        isFinal: false,
        0: {
          transcript: 'foo',
          confidence: 1,
        },
      };
    }),
    resultIndex: 0,
  };
}

class MockSpeechRecognition {
  constructor() {
    this.startCalled = false;
    this.abortCalled = false;
    mockSpeechRecognition = this;
  }

  start() {
    this.startCalled = true;
  }

  abort() {
    this.abortCalled = true;
  }
}

/** @type {!MockSpeechRecognition} */
let mockSpeechRecognition;

suite('NewTabPageVoiceSearchOverlayTest', () => {
  /** @type {!VoiceSearchOverlayElement} */
  let voiceSearchOverlay;

  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  setup(async () => {
    PolymerTest.clearBody();

    window.webkitSpeechRecognition = MockSpeechRecognition;

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    WindowProxy.setInstance(windowProxy);
    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    NewTabPageProxy.setInstance(
        handler, new newTabPage.mojom.PageCallbackRouter());

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
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=waiting]')));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on audio received shows speak text', () => {
    // Act.
    mockSpeechRecognition.onaudiostart();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=speak]')));
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on speech received starts volume animation', () => {
    // Arrange.
    windowProxy.setResultFor('random', 0.5);

    // Act.
    mockSpeechRecognition.onspeechstart();

    // Assert.
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0.5');
  });

  test('on result received shows recognized text', () => {
    // Arrange.
    windowProxy.setResultFor('random', 0.5);
    const result = createResults(2);
    result.results[1][0].confidence = 0;
    result.results[0][0].transcript = 'hello';
    result.results[1][0].transcript = 'world';

    // Act.
    mockSpeechRecognition.onresult(result);

    // Assert.
    const [intermediateResult, finalResult] =
        voiceSearchOverlay.shadowRoot.querySelectorAll(
            '#texts *[text=result] span');
    assertTrue(isVisible(intermediateResult));
    assertTrue(isVisible(finalResult));
    assertEquals(intermediateResult.innerText, 'hello');
    assertEquals(finalResult.innerText, 'world');
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
    result.results[0].isFinal = true;
    result.results[0][0].transcript = 'hello world';

    // Act.
    mockSpeechRecognition.onresult(result);

    // Assert.
    const href = await windowProxy.whenCalled('navigate');
    assertEquals(href, `${googleBaseUrl}/search?q=hello+world&gs_ivs=1`);
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
    assertEquals(
        Action.kQuerySubmitted,
        await handler.whenCalled('onVoiceSearchAction'));
  });

  [['no-speech', 'no-speech', 'learn-more', Error.kNoSpeech],
   ['audio-capture', 'audio-capture', 'learn-more', Error.kAudioCapture],
   ['network', 'network', 'none', Error.kNetwork],
   ['not-allowed', 'not-allowed', 'details', Error.kNotAllowed],
   ['service-not-allowed', 'not-allowed', 'details', Error.kServiceNotAllowed],
   [
     'language-not-supported', 'language-not-supported', 'none',
     Error.kLanguageNotSupported
   ],
   ['aborted', 'other', 'none', Error.kAborted],
   ['bad-grammar', 'other', 'none', Error.kBadGrammar],
   ['foo', 'other', 'none', Error.kOther],
   ['no-match', 'no-match', 'try-again', Error.kNoMatch],
  ].forEach(([error, text, link, logError]) => {
    test(`on '${error}' received shows error text`, async () => {
      // Act.
      if (error === 'no-match') {
        mockSpeechRecognition.onnomatch();
      } else {
        mockSpeechRecognition.onerror({error});
      }

      // Assert.
      assertTrue(isVisible(
          voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
      assertTrue(isVisible(voiceSearchOverlay.shadowRoot.querySelector(
          `#errors *[error="${text}"]`)));
      assertNotStyle(
          voiceSearchOverlay.shadowRoot.querySelector(
              `#errorLinks *[link="${link}"]`),
          'display', 'none');
      assertFalse(
          voiceSearchOverlay.$.micContainer.classList.contains('listening'));
      assertFalse(
          voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
      assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
      assertEquals(logError, await handler.whenCalled('onVoiceSearchError'));
    });
  });

  test('on end received shows error text if no final result', () => {
    // Act.
    mockSpeechRecognition.onend();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
    assertTrue(isVisible(voiceSearchOverlay.shadowRoot.querySelector(
        '#errors *[error="audio-capture"]')));
  });

  test('on end received shows result text if final result', () => {
    // Arrange.
    const result = createResults(1);
    result.results[0].isFinal = true;

    // Act.
    mockSpeechRecognition.onresult(result);
    mockSpeechRecognition.onend();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=result]')));
  });

  const test_params = [
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

  test_params.forEach(function(param) {
    test(`${param.functionName} received resets timer`, async () => {
      // Act.
      // Need to account for previously set timers.
      windowProxy.resetResolver('setTimeout');
      mockSpeechRecognition[param.functionName](...param.arguments);

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
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
  });

  test('on error timeout closes overlay', async () => {
    // Arrange.
    // Need to account for previously set idle timer.
    windowProxy.resetResolver('setTimeout');
    mockSpeechRecognition.onerror({error: 'audio-capture'});

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

  [['#retryLink', Action.kTryAgainLink],
   ['#micButton', Action.kTryAgainMicButton],
  ].forEach(([id, action]) => {
    test(`clicking '${id}' starts voice search if in retry state`, async () => {
      // Arrange.
      mockSpeechRecognition.onnomatch();
      mockSpeechRecognition.startCalled = false;

      // Act.
      $$(voiceSearchOverlay, id).click();

      // Assert.
      assertTrue(mockSpeechRecognition.startCalled);
      assertEquals(action, await handler.whenCalled('onVoiceSearchAction'));
    });
  });

  [' ', 'Enter'].forEach(key => {
    test(`'${key}' submits query if result`, () => {
      // Arrange.
      mockSpeechRecognition.onresult(createResults(1));
      assertEquals(0, windowProxy.getCallCount('navigate'));

      // Act.
      keydown(voiceSearchOverlay.shadowRoot.activeElement, key);

      // Assert.
      assertEquals(1, windowProxy.getCallCount('navigate'));
      assertTrue(voiceSearchOverlay.$.dialog.open);
    });

    test(`'${key}' does not submit query if no result`, () => {
      // Act.
      keydown(voiceSearchOverlay.shadowRoot.activeElement, key);

      // Assert.
      assertEquals(0, windowProxy.getCallCount('navigate'));
      assertTrue(voiceSearchOverlay.$.dialog.open);
    });

    test(`'${key}' triggers link`, () => {
      // Arrange.
      mockSpeechRecognition.onerror({error: 'audio-capture'});
      const link = $$(voiceSearchOverlay, '[link=learn-more]');
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

  test('\'Escape\' closes overlay', async () => {
    // Act.
    keydown(voiceSearchOverlay.shadowRoot.activeElement, 'Escape');

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
    assertEquals(
        Action.kCloseOverlay, await handler.whenCalled('onVoiceSearchAction'));
  });

  test('Click closes overlay', async () => {
    // Act.
    voiceSearchOverlay.$.dialog.click();

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
    assertEquals(
        Action.kCloseOverlay, await handler.whenCalled('onVoiceSearchAction'));
  });

  test('Clicking learn more logs action', async () => {
    // Arrange.
    mockSpeechRecognition.onerror({error: 'audio-capture'});
    const link = $$(voiceSearchOverlay, '[link=learn-more]');
    link.href = '#';
    link.target = '_self';

    // Act.
    link.click();

    // Assert.
    assertEquals(
        Action.kSupportLinkClicked,
        await handler.whenCalled('onVoiceSearchAction'));
  });
});
