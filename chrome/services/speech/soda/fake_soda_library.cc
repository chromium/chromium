// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda/soda_async_impl.h"

namespace {

// Stored in the shared library's .bss segment. If the shared library is
// unloaded (e.g. via FreeLibrary / dlclose) when a SodaClientImpl is destroyed
// and later reloaded from disk, this counter is reset to 0 instead of
// persisting across SodaClientImpl instances.
int g_create_count = 0;

}  // namespace

extern "C" {

#if defined(_WIN32)
#define FAKE_SODA_EXPORT __declspec(dllexport)
#else
#define FAKE_SODA_EXPORT __attribute__((visibility("default")))
#endif

FAKE_SODA_EXPORT void* CreateExtendedSodaAsync(SerializedSodaConfig config) {
  ++g_create_count;
  return reinterpret_cast<void*>(0x1);
}

FAKE_SODA_EXPORT void DeleteExtendedSodaAsync(void* soda_async_handle) {}

FAKE_SODA_EXPORT void ExtendedAddAudio(void* soda_async_handle,
                                       const char* audio_buffer,
                                       int audio_buffer_size) {}

FAKE_SODA_EXPORT void ExtendedSodaMarkDone(void* soda_async_handle) {}

FAKE_SODA_EXPORT void ExtendedSodaStart(void* soda_async_handle) {}

FAKE_SODA_EXPORT int GetFakeSodaLibraryCreateCount() {
  return g_create_count;
}

}  // extern "C"
