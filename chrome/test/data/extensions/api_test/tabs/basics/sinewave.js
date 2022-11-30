// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.sinewave = {
  play: function(win, frequency) {
    win.sinewaveData_ = win.sinewaveData_ || {};
    data = win.sinewaveData_;

    if (!data.audioContext) {
      data.audioContext = new AudioContext();
      data.gainNode = data.audioContext.createGain();
      data.gainNode.gain.value = 0.5;
      data.gainNode.connect(data.audioContext.destination);
    }
    if (!data.oscillator ||
        data.oscillator.frequency.value != frequency) {

      // Note: We recreate the oscillator each time because this switches the
      // audio frequency immediately.  Re-using the same oscillator tends to
      // take several hundred milliseconds to ramp-up/down the frequency.
      if (data.oscillator) {
        data.oscillator.stop();
        data.oscillator.disconnect();
      }
      data.oscillator = data.audioContext.createOscillator();
      data.oscillator.type = OscillatorNode.SINE;
      data.oscillator.frequency.value = frequency;
      data.oscillator.connect(data.gainNode);
    }
    data.oscillator.start();
  },

  stop: function(win) {
    if (win.sinewaveData_)
      win.sinewaveData_.oscillator.stop();
  }
};
