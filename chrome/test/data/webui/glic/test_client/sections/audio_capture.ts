// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from '../page_element_types.js';

class AudioCapture {
  recordedData: Blob[] = [];
  recorder: MediaRecorder|undefined;
  constructor() {}

  async start() {
    if (this.recorder) {
      return;
    }
    try {
      $.audioStatus.innerText = 'Starting Recording...';
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      this.recorder = new MediaRecorder(stream, {mimeType: 'audio/webm'});
      let stopped = false;
      window.setInterval(() => {
        if (!stopped) {
          this.recorder!.requestData();
        }
      }, 100);
      this.recorder.addEventListener('dataavailable', (event: BlobEvent) => {
        this.recordedData.push(event.data);
      });
      this.recorder.addEventListener('stop', () => {
        stopped = true;
        $.audioStatus.innerText = 'Recording Stopped';
        const blob = new Blob(this.recordedData, {type: 'audio/webm'});
        $.mic.src = URL.createObjectURL(blob);
      });
      this.recorder.start();
      $.audioStatus.innerText = 'Recording...';
    } catch (error) {
      $.audioStatus.innerText = `Caught error: ${error}`;
    }
  }

  stop() {
    if (!this.recorder) {
      return;
    }
    $.mic.play();
    this.recorder.stop();
    this.recorder = undefined;
  }
}
const audioCapture = new AudioCapture();

$.audioCapStop.addEventListener('click', () => {
  audioCapture.stop();
});
$.audioCapStart.addEventListener('click', () => {
  audioCapture.start();
});
