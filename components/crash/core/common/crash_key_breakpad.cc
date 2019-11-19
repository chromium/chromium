// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This file is only compiled when Crashpad is not used as the crash
// reproter.

#include "components/crash/core/common/crash_key.h"

#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key_base_support.h"
#include "components/crash/core/common/crash_key_internal.h"

#if defined(OS_MACOSX) || defined(OS_IOS) || defined(OS_WIN)
#error "This file should not be used when Crashpad is available, nor on iOS."
#endif

namespace crash_reporter {
namespace internal {

namespace {

// String used to format chunked key names. The __1 through __N syntax is
// recognized by the crash collector, which will then stitch the numbered
// parts back into a single string value.
const char kChunkFormatString[] = "%s__%" PRIuS;

static TransitionalCrashKeyStorage* g_storage = nullptr;

constexpr size_t kUnsetStorageSlotSentinel =
    TransitionalCrashKeyStorage::num_entries;

}  // namespace

TransitionalCrashKeyStorage* GetCrashKeyStorage() {
  if (!g_storage) {
    g_storage = new internal::TransitionalCrashKeyStorage();
  }
  return g_storage;
}

void ResetCrashKeyStorageForTesting() {
  auto* storage = g_storage;
  g_storage = nullptr;
  delete storage;
}

void CrashKeyStringImpl::Set(base::StringPiece value) {
  const size_t kValueMaxLength = index_array_count_ * kCrashKeyStorageValueSize;

  TransitionalCrashKeyStorage* storage = GetCrashKeyStorage();

  value = value.substr(0, kValueMaxLength);

  // If there is only one slot for the value, then handle it directly.
  if (index_array_count_ == 1) {
    std::string value_string = value.as_string();
    if (is_set()) {
      storage->SetValueAtIndex(index_array_[0], value_string.c_str());
    } else {
      index_array_[0] = storage->SetKeyValue(name_, value_string.c_str());
    }
    return;
  }

  // If the value fits in a single slot, the name of the key should not
  // end with the __1 suffix of the chunked format.
  if (value.length() < kCrashKeyStorageValueSize - 1) {
    if (index_array_[1] != kUnsetStorageSlotSentinel) {
      // If switching from chunked to non-chunked, clear all the values.
      Clear();
      index_array_[0] = storage->SetKeyValue(name_, value.data());
    } else if (index_array_[0] != kUnsetStorageSlotSentinel) {
      // The single entry was previously set.
      storage->SetValueAtIndex(index_array_[0], value.data());
    } else {
      // This key was not previously set.
      index_array_[0] = storage->SetKeyValue(name_, value.data());
    }
    return;
  }

  // If the key was previously set, but only using one slot, then the chunk
  // name will change (from |name| to |name__1|).
  if (index_array_[0] != kUnsetStorageSlotSentinel &&
      index_array_[1] == kUnsetStorageSlotSentinel) {
    storage->RemoveAtIndex(index_array_[0]);
    index_array_[0] = kUnsetStorageSlotSentinel;
  }

  // Otherwise, break the value into chunks labeled name__1 through name__N,
  // where N is |index_array_count_|.
  size_t offset = 0;
  for (size_t i = 0; i < index_array_count_; ++i) {
    if (offset < value.length()) {
      // The storage NUL-terminates the value, so ensure that a byte is
      // not lost when setting individaul chunks.
      base::StringPiece chunk =
          value.substr(offset, kCrashKeyStorageValueSize - 1);
      offset += chunk.length();

      if (index_array_[i] == kUnsetStorageSlotSentinel) {
        std::string chunk_name =
            base::StringPrintf(kChunkFormatString, name_, i + 1);
        index_array_[i] =
            storage->SetKeyValue(chunk_name.c_str(), chunk.data());
      } else {
        storage->SetValueAtIndex(index_array_[i], chunk.data());
      }
    } else {
      storage->RemoveAtIndex(index_array_[i]);
      index_array_[i] = kUnsetStorageSlotSentinel;
    }
  }
}

void CrashKeyStringImpl::Clear() {
  for (size_t i = 0; i < index_array_count_; ++i) {
    GetCrashKeyStorage()->RemoveAtIndex(index_array_[i]);
    index_array_[i] = kUnsetStorageSlotSentinel;
  }
}

bool CrashKeyStringImpl::is_set() const {
  return index_array_[0] != kUnsetStorageSlotSentinel;
}

}  // namespace internal

void InitializeCrashKeys() {
  internal::GetCrashKeyStorage();
  InitializeCrashKeyBaseSupport();
}

std::string GetCrashKeyValue(const std::string& key_name) {
  const char* value =
      internal::GetCrashKeyStorage()->GetValueForKey(key_name.c_str());
  if (value)
    return value;
  return std::string();
}

void InitializeCrashKeysForTesting() {
  InitializeCrashKeys();
}

void ResetCrashKeysForTesting() {
  internal::ResetCrashKeyStorageForTesting();
  base::debug::SetCrashKeyImplementation(nullptr);
}

}  // namespace crash_reporter
