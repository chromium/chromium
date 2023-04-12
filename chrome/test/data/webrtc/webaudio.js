/**
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gContext = null;

function loadAudioAndAddToPeerConnection(url, peerconnection) {
  if (gContext == null)
    gContext = new AudioContext();

  var inputSink = gContext.createMediaStreamDestination();
  peerconnection.addStream(inputSink.stream);

  return new Promise(resolve => {
    loadAudioBuffer_(url, resolve);
  }).then(function(voiceSoundBuffer) {
    if (peerconnection.webAudioBufferSource)
      throw new Error('Cannot load more than one sound per peerconnection.');

    peerconnection.webAudioBufferSource = gContext.createBufferSource();
    peerconnection.webAudioBufferSource.buffer = voiceSoundBuffer;
    peerconnection.webAudioBufferSource.connect(inputSink);
    return logAndReturn('ok-added');
  });
}

function playPreviouslyLoadedAudioFile(peerconnection) {
  if (!peerconnection.webAudioBufferSource)
    throw new Error('Must call loadAudioAndAddToPeerConnection before this.');
  peerconnection.webAudioBufferSource.start(gContext.currentTime);
}

/** @private */
function loadAudioBuffer_(url, callback) {
  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.responseType = 'arraybuffer';

  request.onload = function() {
    gContext.decodeAudioData(request.response, function (decodedAudio) {
          voiceSoundBuffer = decodedAudio;
          callback(voiceSoundBuffer);
        });
  }
  request.send();
}
