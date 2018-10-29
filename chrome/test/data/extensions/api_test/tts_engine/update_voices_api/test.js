// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setup() {
  var speakListener = function(utterance, options, sendTtsEvent) {};
  var stopListener = function() {};
  chrome.ttsEngine.onSpeak.addListener(speakListener);
  chrome.ttsEngine.onStop.addListener(stopListener);
}

chrome.test.runTests([
  function testGetVoices() {
    var testVoiceData = [
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
    setup();
    chrome.tts.getVoices(function(voices) {
      chrome.test.assertEq(1, voices.length);
      chrome.test.assertEq({
        eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
        extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
        lang: 'en-US',
        remote: false,
        voiceName: 'Zach'
      }, voices[0]);
      chrome.ttsEngine.updateVoices(testVoiceData);
      chrome.tts.getVoices(function(runtimeVoices) {
        chrome.test.assertEq(testVoiceData.length, runtimeVoices.length);
        for (var i = 0; i < runtimeVoices.length; i++) {
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
  function testExtensionIdMismatch() {
    setup();
    chrome.ttsEngine.updateVoices([]);

    chrome.ttsEngine.updateVoices([{
      eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
      extensionId: 'interloper',
      lang: 'en-US',
      remote: false,
      voiceName: 'Zach'
    }]);

    chrome.tts.getVoices(function(voices) {
      chrome.test.assertEq(0, voices.length);
      chrome.test.succeed();
    });
  },
  function testInvalidLang() {
    setup();
    chrome.ttsEngine.updateVoices([{
      eventTypes: [ 'end', 'interrupted', 'cancelled', 'error'],
      extensionId: 'pkplfbidichfdicaijlchgnapepdginl',
      lang: 'bad lang syntax',
      remote: false,
      voiceName: 'Zach'
    }]);

    chrome.tts.getVoices(function(voices) {
      chrome.test.assertEq(0, voices.length);
      chrome.test.succeed();
    });
  }
]);
