// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

setup = () => {
  const speakListener = (utterance, options, sendTtsEvent) => {};
  const stopListener = () => {};
  chrome.ttsEngine.onSpeak.addListener(speakListener);
  chrome.ttsEngine.onStop.addListener(stopListener);
}

const testVoiceData = [
  {
    eventTypes: ['start'],
    extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
    lang: 'zh-TW',
    remote: false,
    voiceName: 'David'
  },
  {
    eventTypes: ['end', 'interrupted', 'cancelled'],
    extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
    gender: 'female',
    lang: 'en-GB',
    remote: false,
    voiceName: 'Laura'
  }
];

chrome.test.runTests([
  testGetVoices = () => {
    setup();
    chrome.tts.getVoices((voices) => {
      chrome.test.assertEq(1, voices.length);
      chrome.test.assertEq({
        eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
        extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
        lang: 'en-US',
        remote: false,
        voiceName: 'Zach'
      }, voices[0]);
      chrome.ttsEngine.updateVoices(testVoiceData);
      chrome.tts.getVoices((runtimeVoices) => {
        chrome.test.assertEq(testVoiceData.length, runtimeVoices.length);
        for (let i = 0; i < runtimeVoices.length; i++) {
          // The result should not have 'gender'.
          delete testVoiceData[i]['gender'];
          chrome.test.assertEq(testVoiceData[i], runtimeVoices[i]);
          chrome.test.assertEq(runtimeVoices[i], testVoiceData[i]);
        }
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  testExtensionIdMismatch = () => {
    setup();
    chrome.ttsEngine.updateVoices([]);

    chrome.ttsEngine.updateVoices([{
      eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
      extensionId: 'interloper',
      lang: 'en-US',
      remote: false,
      voiceName: 'Zach'
    }]);

    chrome.tts.getVoices((voices) => {
      chrome.test.assertEq(0, voices.length);
      chrome.test.succeed();
    });
  },
  testInvalidLang = () => {
    setup();
    chrome.ttsEngine.updateVoices([{
      eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
      extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
      lang: 'bad lang syntax',
      remote: false,
      voiceName: 'Zach'
    }]);

    chrome.tts.getVoices((voices) => {
      chrome.test.assertEq(0, voices.length);
      chrome.test.succeed();
    });
  },
  testAddVoicesCallsVoicesChangedListener = () => {
    chrome.tts.onVoicesChanged.addListener(() => {
        // Should happen sometime after updateVoices is called,
        // but it isn't required to happen before or after
        // a getVoices call would return, so we will check that
        // getVoices returns the right data separately.
        chrome.tts.getVoices((runtimeVoices) => {
          chrome.test.assertEq(testVoiceData.length, runtimeVoices.length);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    });
    chrome.ttsEngine.updateVoices(testVoiceData);
  },
]);
