// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SpeechBrowserProxyImpl, WebSpeechTtsClient} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('WebSpeechTtsClient', () => {
  let client: WebSpeechTtsClient;
  let speechBrowserProxy: TestSpeechBrowserProxy;

  setup(() => {
    speechBrowserProxy = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speechBrowserProxy);

    client = WebSpeechTtsClient.initialize();
  });

  test('play calls speak on synth', async () => {
    client.play({text: 'hello world'});
    const utterance = await speechBrowserProxy.whenCalled('speak');
    assertEquals('hello world', utterance.text);
  });

  test('pause pauses synth', () => {
    client.pause();
    assertEquals(1, speechBrowserProxy.getCallCount('pause'));
  });

  test('stop cancels synth', () => {
    client.stop();
    assertEquals(1, speechBrowserProxy.getCallCount('cancel'));
  });

  test('getVoices returns voices', () => {
    const voice1 = createSpeechSynthesisVoice({
      default: true,
      name: 'Google Eitan',
      localService: true,
      lang: 'en-us',
    });
    speechBrowserProxy.setVoices([voice1]);

    const voices = client.getVoices();
    assertEquals(1, voices.length);
    assertEquals(voice1, voices[0]);
  });

  test('getVoices filters out network voices if local voices exist', () => {
    const localVoice = createSpeechSynthesisVoice({
      name: 'Local Voice',
      localService: true,
      lang: 'en-us',
    });
    const networkVoice = createSpeechSynthesisVoice({
      name: 'Network Voice',
      localService: false,
      lang: 'en-us',
    });
    speechBrowserProxy.setVoices([localVoice, networkVoice]);

    const voices = client.getVoices();
    assertEquals(1, voices.length);
    assertEquals(localVoice, voices[0]);
  });
});
