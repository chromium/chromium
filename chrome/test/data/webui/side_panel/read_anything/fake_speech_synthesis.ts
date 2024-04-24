// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A fake SpeechSynthesis object for testing
export class FakeSpeechSynthesis {
  canceled: boolean = false;
  paused: boolean = false;
  speaking: boolean = false;
  spokenUtterances: SpeechSynthesisUtterance[];
  errorEvent: SpeechSynthesisErrorCode|undefined;


  constructor() {
    this.spokenUtterances = [];
  }

  cancel() {
    this.canceled = true;
    this.speaking = false;
    this.spokenUtterances = [];
  }

  pause() {
    this.paused = true;
  }

  resume() {
    this.paused = false;
    this.speaking = true;
  }

  getVoices(): SpeechSynthesisVoice[] {
    return [
      {lang: 'en', name: 'Lauren', default: true} as SpeechSynthesisVoice,
      {lang: 'en', name: 'Eitan'} as SpeechSynthesisVoice,
      {lang: 'en', name: 'Kristi'} as SpeechSynthesisVoice,
      {lang: 'en', name: 'Shari'} as SpeechSynthesisVoice,
      {lang: 'en', name: 'Yu'} as SpeechSynthesisVoice,
      {lang: 'en', name: 'Xiang'} as SpeechSynthesisVoice,
    ];
  }

  speak(utterance: SpeechSynthesisUtterance) {
    this.paused = false;
    this.speaking = true;
    this.spokenUtterances.push(utterance);
    if (utterance.onend) {
      utterance.onend(new SpeechSynthesisEvent('end', {utterance}));
    }

    if (utterance.onerror && this.errorEvent !== undefined) {
      utterance.onerror(new SpeechSynthesisErrorEvent(
          'type', {utterance: utterance, error: this.errorEvent}));
      this.errorEvent = undefined;
    }
  }

  // On the next #speak, trigger an error of the given error code.
  triggerErrorEventOnNextSpeak(errorCode: SpeechSynthesisErrorCode): void {
    this.errorEvent = errorCode;
  }

  // These are currently unused in tests but need to be defined in order to be
  // used a SpeechSynthesis object. Can be implemented as needed.
  onvoiceschanged = () => {};
  pending = false;
  addEventListener = () => {};
  removeEventListener = () => {};
  dispatchEvent = () => false;
}
