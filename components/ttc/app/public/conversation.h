// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_CONVERSATION_H_
#define COMPONENTS_TTC_APP_PUBLIC_CONVERSATION_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/ttc/app/public/page_context_types.h"

namespace media {
class AudioParameters;
}  // namespace media

namespace ttc {

class SessionJournal;

// Manages a voice/multimodal conversation session with the TTC model.
// Wires audio input/output to the model backend for bidirectional streaming,
// handling speech capture, audio playback, interruptions, transcripts, and
// tools.
class Conversation {
 public:
  virtual ~Conversation() = default;

  // Starts the conversation session: connects to backend and begins mic
  // capture.
  virtual void Start() = 0;

  // Stops the conversation session: closes backend connection and stops audio.
  virtual void Stop() = 0;

  // Sends user text, context, or tool messages to the model.
  virtual void SendTextInput(const std::string& text) = 0;

  // Sends an active-tab page context update (either shared tab metadata +
  // optional content, or a restricted-page marker) to the model.
  virtual void SendContextUpdate(PageContextUpdate update) = 0;

  // Invoked when the page the session is operating on has changed, and so the
  // conversation's page context may be stale.
  virtual void OnPageContextInvalidated() = 0;

  // Mutes or unmutes the user's microphone in TTC (as opposed to the OS or
  // hardware muting it). While muted no audio is captured or sent to the model;
  // the model can still finish a reply in progress.
  virtual void SetMicMuted(bool muted) = 0;

  // Routes captured microphone PCM frames to `sink` instead of the TTC model
  // backend while a Human-on-the-Loop (HOTL) confirmation is active.
  // Activating a non-null sink clears queued model playback, completes playback
  // reporting for any active turn, and drops incoming model audio so the sink
  // hears only the user. Passing a null callback restores normal routing.
  using HotlAudioSinkCallback =
      base::RepeatingCallback<void(base::span<const int16_t> pcm_data,
                                   const media::AudioParameters& params)>;
  virtual void SetHotlAudioSink(HotlAudioSinkCallback sink) = 0;

  // Returns the journal for recording events in this session.
  virtual SessionJournal& GetJournal() = 0;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_CONVERSATION_H_
