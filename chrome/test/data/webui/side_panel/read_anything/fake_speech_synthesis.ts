// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {createSpeechSynthesisVoice} from './common.js';

// A fake SpeechSynthesis object for testing
export class FakeSpeechSynthesis {
  canceled: boolean = false;
  paused: boolean = false;
  speaking: boolean = false;
  spokenUtterances: SpeechSynthesisUtterance[];
  errorEvent: SpeechSynthesisErrorCode|undefined;
  triggerUtteranceStartedEventNext: boolean = false;
  shouldUseLocalVoices: boolean = false;
  canceledUtterances: SpeechSynthesisUtterance[];
  currentUtterance: SpeechSynthesisUtterance|undefined;
  private voices_: SpeechSynthesisVoice[] = [];


  constructor() {
    this.spokenUtterances = [];
    this.canceledUtterances = [];
  }

  clearSpokenUtterances() {
    this.spokenUtterances = [];
    this.canceledUtterances = [];
  }

  cancel() {
    this.canceled = true;
    this.speaking = false;

    if (this.currentUtterance) {
      this.canceledUtterances.push(this.currentUtterance);
      this.currentUtterance = undefined;
    }
  }

  pause() {
    this.paused = true;
  }

  resume() {
    this.paused = false;
    this.speaking = true;
  }

  getVoices(): SpeechSynthesisVoice[] {
    return this.voices_.length > 0 ? this.voices_ : [
      createSpeechSynthesisVoice({lang: 'en', name: 'Lauren', default: true}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Eitan'}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Kristi'}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Shari'}),
      createSpeechSynthesisVoice({lang: 'en', name: 'Yu'}),
      createSpeechSynthesisVoice(
          {lang: 'en', name: 'Xiang', localService: this.shouldUseLocalVoices}),
    ];
  }

  setVoices(voices: SpeechSynthesisVoice[]) {
    this.voices_ = voices;
  }

  speak(utterance: SpeechSynthesisUtterance) {
    this.currentUtterance = utterance;
    this.paused = false;
    this.speaking = true;
    this.spokenUtterances.push(utterance);

    if (utterance.onerror && this.errorEvent !== undefined) {
      // Copy this.errorEvent to a temporary variable so that this.errorEvent
      // can be cleared before calling onerror. Otherwise, we can create an
      // infinite loop where this.errorEvent never clears based on how
      // synthesis is handled in the main app.
      const errorCode = this.errorEvent;
      this.errorEvent = undefined;
      utterance.onerror(new SpeechSynthesisErrorEvent(
          'type', {utterance: utterance, error: errorCode}));
    }

    if (this.triggerUtteranceStartedEventNext && utterance.onstart) {
      utterance.onstart(new SpeechSynthesisEvent('start', {utterance}));
      this.triggerUtteranceStartedEventNext = false;
    }

    if (utterance.onend) {
      this.currentUtterance = undefined;
      utterance.onend(new SpeechSynthesisEvent('end', {utterance}));
    }
  }

  // On the next #speak, trigger an error of the given error code.
  triggerErrorEventOnNextSpeak(errorCode: SpeechSynthesisErrorCode): void {
    this.errorEvent = errorCode;
  }

  triggerUtteranceStartedOnNextSpeak() {
    this.triggerUtteranceStartedEventNext = true;
  }

  useLocalVoices() {
    this.shouldUseLocalVoices = true;
  }

  // These are currently unused in tests but need to be defined in order to be
  // used a SpeechSynthesis object. Can be implemented as needed.
  onvoiceschanged = () => {};
  pending = false;
  addEventListener = () => {};
  removeEventListener = () => {};
  dispatchEvent = () => false;
}
