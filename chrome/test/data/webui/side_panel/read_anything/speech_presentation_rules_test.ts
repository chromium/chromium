// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioBrowserProxyImpl, getCurrentSpeechRate, isInvalidHighlightForWordHighlighting, textEndsWithOpeningPunctuation} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';

suite('SpeechPresentationRules', () => {
  let audioProxy: TestAudioBrowserProxy;

  setup(() => {
    audioProxy = new TestAudioBrowserProxy();
    AudioBrowserProxyImpl.setInstance(audioProxy);
  });

  test('getCurrentSpeechRate rounds value to 1 decimal', () => {
    audioProxy.speechRate = 1.1234567890;
    assertEquals(1.1, getCurrentSpeechRate());

    audioProxy.speechRate = 0.912345678;
    assertEquals(0.9, getCurrentSpeechRate());

    audioProxy.speechRate = 1.199999999;
    assertEquals(1.2, getCurrentSpeechRate());
  });

  // <if expr="not is_chromeos">
  test('getCurrentSpeechRate caps at value to 2.0 on Desktop', () => {
    audioProxy.speechRate = 4.0;
    assertEquals(2.0, getCurrentSpeechRate());

    audioProxy.speechRate = 3.0;
    assertEquals(2.0, getCurrentSpeechRate());

    audioProxy.speechRate = 2.1;
    assertEquals(2.0, getCurrentSpeechRate());

    audioProxy.speechRate = 2.0;
    assertEquals(2.0, getCurrentSpeechRate());

    // Values below 2.0 aren't impacted.
    audioProxy.speechRate = 1.199999;
    assertEquals(1.2, getCurrentSpeechRate());

    audioProxy.speechRate = .53333;
    assertEquals(.5, getCurrentSpeechRate());
  });
  // </if>

  // <if expr="is_chromeos">
  test('getCurrentSpeechRate does not cap value on ChromeOS', () => {
    audioProxy.speechRate = 4.0;
    assertEquals(4.0, getCurrentSpeechRate());

    audioProxy.speechRate = 3.0;
    assertEquals(3.0, getCurrentSpeechRate());

    audioProxy.speechRate = 2.1;
    assertEquals(2.1, getCurrentSpeechRate());

    audioProxy.speechRate = 2.0;
    assertEquals(2.0, getCurrentSpeechRate());
  });
  // </if>


  test('isInvalidHighlightForWordHighlighting', () => {
    assertTrue(isInvalidHighlightForWordHighlighting());
    assertTrue(isInvalidHighlightForWordHighlighting(''));
    assertTrue(isInvalidHighlightForWordHighlighting(' '));
    assertTrue(isInvalidHighlightForWordHighlighting('  '));
    assertTrue(isInvalidHighlightForWordHighlighting('!'));
    assertTrue(isInvalidHighlightForWordHighlighting('()?!?'));
    assertFalse(isInvalidHighlightForWordHighlighting('hello !!!'));
    assertFalse(isInvalidHighlightForWordHighlighting('(psst);'));
  });

  test('speechEndsWithOpeningPunctuation', () => {
    assertNull(textEndsWithOpeningPunctuation(' '));
    assertNull(textEndsWithOpeningPunctuation('how()'));
    assertEquals('[', textEndsWithOpeningPunctuation('[')![0]);
    assertEquals('[', textEndsWithOpeningPunctuation('hello[')![0]);
    assertEquals('{', textEndsWithOpeningPunctuation('goodbye{')![0]);
    assertEquals('<', textEndsWithOpeningPunctuation('where?<')![0]);
    assertEquals('(', textEndsWithOpeningPunctuation('why(')![0]);
    assertEquals('((', textEndsWithOpeningPunctuation('when((')![0]);
  });
});
