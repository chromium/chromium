// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_ASYNC_IMPL_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_ASYNC_IMPL_H_

// This file contains the interface contract between Chrome and the Speech
// On-Device API (SODA) binary. Changes to this interface must be backwards
// compatible since the SODA binary on the device may be older or newer than the
// Chrome version.

extern "C" {

typedef void (*RecognitionResultHandler)(const char*, const bool, void*);
typedef void (*SerializedSodaEventHandler)(const char*, int, void*);

typedef struct {
  // The channel count and sample rate of the audio stream. SODA does not
  // support changing these values mid-stream, so a new SODA instance must be
  // created if these values change.
  int channel_count;
  int sample_rate;

  // The fully-qualified path to the language pack.
  const char* language_pack_directory;

  // The callback that gets executed on a recognition event. It takes in a
  // char*, representing the transcribed text; a bool, representing whether the
  // result is final or not; and a void pointer to the object that is associated
  // with the callback.
  RecognitionResultHandler callback;

  // A void pointer to the object that is associated with the callback.
  // Ownership is not taken.
  void* callback_handle;

  // The API key used to verify that the binary is called by Chrome.
  const char* api_key;
} SodaConfig;

typedef struct {
  // A ExtendedSodaConfigMsg that's been serialized as a string. Not owned.
  const char* soda_config;

  // length of char* in soda_config.
  int soda_config_size;

  // The callback that gets executed on a SODA event. It takes in a
  // char*, which is a serialized SodaResponse proto, an int specifying the
  // length of the char* and a void pointer to the object that is associated
  // with the callback.
  SerializedSodaEventHandler callback;

  // A void pointer to the object that is associated with the callback.
  void* callback_handle;
} SerializedSodaConfig;

void* CreateSoda(SerializedSodaConfig config);

// Destroys the instance of SODA, called on the destruction of the SodaClient.
void DeleteSodaAsync(void* soda_async_handle);

// Feeds raw audio to SODA in the form of a contiguous stream of characters.
void AddAudio(void* soda_async_handle,
              const char* audio_buffer,
              int audio_buffer_size);
}

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_ASYNC_IMPL_H_
