// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The type of transcription update.
enum TranscriptionType {
  "partial",
  "final"
};

// The current state of the dictation stream.
enum StreamState {
  "initializing",
  "failed",
  "transcribing",
  "complete"
};

dictionary DictationContext {
  // The captured AnnotatedPageContent proto.
  ArrayBuffer annotatedPageContent;
  // The plain text of the page.
  DOMString innerText;
  // The existing content of the editable field.
  DOMString editableContent;
};

dictionary StartStreamFlags {
  // Starts the stream in "eval" mode, useful for recording inputs.
  required boolean evalMode;
};

dictionary StartStreamDetails {
  // The unique identifier of the stream.
  required long streamId;
  // The context of the dictation session. May be omitted if context is passed
  // asynchronously in the ContextUpdate event instead.
  DictationContext context;
  // Flags used to experimentally modify the behavior of the extension.
  required StartStreamFlags flags;
};

callback OnStartStreamListener = undefined (StartStreamDetails details);

interface OnStartStreamEvent : ExtensionEvent {
  static undefined addListener(OnStartStreamListener listener);
  static undefined removeListener(OnStartStreamListener listener);
  static boolean hasListener(OnStartStreamListener listener);
};

dictionary EndStreamDetails {
  // The unique identifier of the stream.
  required long streamId;
};

callback OnEndStreamListener = undefined (EndStreamDetails details);

interface OnEndStreamEvent : ExtensionEvent {
  static undefined addListener(OnEndStreamListener listener);
  static undefined removeListener(OnEndStreamListener listener);
  static boolean hasListener(OnEndStreamListener listener);
};

dictionary ContextUpdateDetails {
  // The unique identifier of the stream.
  required long streamId;
  // The context of the dictation session.
  required DictationContext context;
};

callback OnContextUpdateListener = undefined (ContextUpdateDetails details);

interface OnContextUpdateEvent : ExtensionEvent {
  static undefined addListener(OnContextUpdateListener listener);
  static undefined removeListener(OnContextUpdateListener listener);
  static boolean hasListener(OnContextUpdateListener listener);
};

dictionary UpdateTranscriptionDetails {
  // The unique identifier of the dictation stream.
  required long streamId;
  // The type of transcription update.
  required TranscriptionType type;
  // The transcribed text data.
  required DOMString data;
};

dictionary SetStreamStateDetails {
  required long streamId;
  required StreamState state;
};

// The dictationPrivate API is a private API used by the dictation extension.
interface DictationPrivate {
  // Sends the transcription to the browser.
  static Promise<undefined> updateTranscription(
      UpdateTranscriptionDetails details);

  // Notifies the browser of stream state changes.
  static Promise<undefined> setStreamState(SetStreamStateDetails details);

  // Fired to instruct the extension to start capturing microphone audio and
  // connecting to transcription service.
  static attribute OnStartStreamEvent onStartStream;

  // Fired to instruct the extension to finish transcribing.
  static attribute OnEndStreamEvent onEndStream;

  // Fired to provide updated context for an active stream. Only sent if
  // context wasn't provided as part of the StartStream event.
  static attribute OnContextUpdateEvent onContextUpdate;
};

partial interface Browser {
  static attribute DictationPrivate dictationPrivate;
};
