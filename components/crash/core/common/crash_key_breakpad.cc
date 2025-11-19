// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This file is only compiled when Crashpad is not used as the crash
// reproter.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string_view>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_key_base_support.h"
#include "components/crash/core/common/crash_key_internal.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
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

void CrashKeyStringImpl::Set(std::string_view value) {
  // This check cannot be in the constructor because it is constexpr. Use _LT
  // rather than _LE to account for the terminating \0.
  DCHECK_LT(strlen(name_), kCrashKeyStorageKeySize);

  const size_t kValueMaxLength = indexes_.size() * kCrashKeyStorageValueSize;

  TransitionalCrashKeyStorage* storage = GetCrashKeyStorage();

  value = value.substr(0, kValueMaxLength);

  // If there is only one slot for the value, then handle it directly.
  if (indexes_.size() == 1) {
    std::string value_string(value);
    if (is_set()) {
      storage->SetValueAtIndex(indexes_[0], value_string.c_str());
    } else {
      indexes_[0] = storage->SetKeyValue(name_, value_string.c_str());
    }
    return;
  }

  // If the value fits in a single slot, the name of the key should not
  // end with the __1 suffix of the chunked format.
  if (value.length() < kCrashKeyStorageValueSize - 1) {
    if (indexes_[1] != kUnsetStorageSlotSentinel) {
      // If switching from chunked to non-chunked, clear all the values.
      Clear();
      indexes_[0] = storage->SetKeyValue(name_, value.data());
    } else if (indexes_[0] != kUnsetStorageSlotSentinel) {
      // The single entry was previously set.
      storage->SetValueAtIndex(indexes_[0], value.data());
    } else {
      // This key was not previously set.
      indexes_[0] = storage->SetKeyValue(name_, value.data());
    }
    return;
  }

  // If the key was previously set, but only using one slot, then the chunk
  // name will change (from |name| to |name__1|).
  if (indexes_[0] != kUnsetStorageSlotSentinel &&
      indexes_[1] == kUnsetStorageSlotSentinel) {
    storage->RemoveAtIndex(indexes_[0]);
    indexes_[0] = kUnsetStorageSlotSentinel;
  }

  // Otherwise, break the value into chunks labeled name__1 through name__N,
  // where N is |indexes_.size()|.
  size_t offset = 0;
  for (size_t i = 0; i < indexes_.size(); ++i) {
    if (offset >= value.length()) {
      storage->RemoveAtIndex(indexes_[i]);
      indexes_[i] = kUnsetStorageSlotSentinel;
      continue;
    }

    // The storage NUL-terminates the value, so ensure that a byte is
    // not lost when setting individual chunks.
    std::string_view chunk =
        value.substr(offset, kCrashKeyStorageValueSize - 1);
    offset += chunk.length();

    if (indexes_[i] == kUnsetStorageSlotSentinel) {
      std::string chunk_name =
          base::StringPrintf(kChunkFormatString, name_, i + 1);
      indexes_[i] = storage->SetKeyValue(chunk_name.c_str(), chunk.data());
    } else {
      storage->SetValueAtIndex(indexes_[i], chunk.data());
    }
  }
}

void CrashKeyStringImpl::Clear() {
  for (size_t& index : indexes_) {
    GetCrashKeyStorage()->RemoveAtIndex(index);
    index = kUnsetStorageSlotSentinel;
  }
}

bool CrashKeyStringImpl::is_set() const {
  return indexes_[0] != kUnsetStorageSlotSentinel;
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
