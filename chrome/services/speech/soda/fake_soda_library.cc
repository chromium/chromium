// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda/soda_async_impl.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace {

// Stored in the shared library's .bss segment. If the shared library is
// unloaded (e.g. via FreeLibrary / dlclose) when a SodaClientImpl is destroyed
// and later reloaded from disk, this counter is reset to 0 instead of
// persisting across SodaClientImpl instances.
int g_create_count = 0;

// Serialized SodaResponse with soda_type = STOP (2).
constexpr char kStopResponse[] = {0x08, 0x02};

struct FakeSoda {
  SerializedSodaConfig config = {};
  bool running = true;
};

#if defined(__clang__)
__attribute__((no_sanitize("cfi-icall")))
#endif
void EmitStop(FakeSoda* soda) {
  soda->config.callback(kStopResponse, sizeof(kStopResponse),
                        soda->config.callback_handle);
}

#if defined(_WIN32)
DWORD WINAPI EmitStopOnThread(LPVOID soda) {
  EmitStop(static_cast<FakeSoda*>(soda));
  return 0;
}
#else
void* EmitStopOnThread(void* soda) {
  EmitStop(static_cast<FakeSoda*>(soda));
  return nullptr;
}
#endif

// Emits STOP on a separate thread and waits for it to finish, matching SODA.
void Stop(FakeSoda* soda) {
  if (!soda->running) {
    return;
  }
  soda->running = false;

  if (!soda->config.callback) {
    return;
  }

#if defined(_WIN32)
  HANDLE thread =
      ::CreateThread(nullptr, 0, &EmitStopOnThread, soda, 0, nullptr);
  if (thread) {
    ::WaitForSingleObject(thread, INFINITE);
    ::CloseHandle(thread);
  }
#else
  pthread_t thread;
  if (pthread_create(&thread, nullptr, &EmitStopOnThread, soda) == 0) {
    pthread_join(thread, nullptr);
  }
#endif
}

}  // namespace

extern "C" {

#if defined(_WIN32)
#define FAKE_SODA_EXPORT __declspec(dllexport)
#else
#define FAKE_SODA_EXPORT __attribute__((visibility("default")))
#endif

FAKE_SODA_EXPORT void* CreateExtendedSodaAsync(SerializedSodaConfig config) {
  ++g_create_count;
  config.soda_config = nullptr;
  config.soda_config_size = 0;
  return new FakeSoda{config};
}

FAKE_SODA_EXPORT void DeleteExtendedSodaAsync(void* soda_async_handle) {
  FakeSoda* soda = static_cast<FakeSoda*>(soda_async_handle);
  if (!soda) {
    return;
  }
  Stop(soda);
  delete soda;
}

FAKE_SODA_EXPORT void ExtendedAddAudio(void* soda_async_handle,
                                       const char* audio_buffer,
                                       int audio_buffer_size) {}

FAKE_SODA_EXPORT void ExtendedSodaMarkDone(void* soda_async_handle) {
  if (soda_async_handle) {
    Stop(static_cast<FakeSoda*>(soda_async_handle));
  }
}

FAKE_SODA_EXPORT void ExtendedSodaStart(void* soda_async_handle) {}

FAKE_SODA_EXPORT int GetFakeSodaLibraryCreateCount() {
  return g_create_count;
}

}  // extern "C"
