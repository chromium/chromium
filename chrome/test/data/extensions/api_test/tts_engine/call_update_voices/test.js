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
  function testSetUpDynamicVoices() {
    var testVoiceData = [
      {
        eventTypes: ['start'],
        lang: 'zh-TW',
        remote: false,
        voiceName: 'Dynamic Voice 1'
      },
      {
        eventTypes: ['end', 'interrupted', 'cancelled'],
        lang: 'en-GB',
        remote: false,
        voiceName: 'Dynamic Voice 2'
      }
    ];
    setup();
    chrome.ttsEngine.updateVoices(testVoiceData);
    chrome.test.succeed();
  }
]);
