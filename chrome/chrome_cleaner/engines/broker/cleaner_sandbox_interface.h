// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_SANDBOX_INTERFACE_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_SANDBOX_INTERFACE_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"

namespace chrome_cleaner_sandbox {

enum class TerminateProcessResult {
  kSuccess,
  kFailed,
  kDenied,
};

bool SandboxNtDeleteRegistryKey(
    const chrome_cleaner::String16EmbeddedNulls& key);

bool SandboxNtDeleteRegistryValue(
    const chrome_cleaner::String16EmbeddedNulls& key,
    const chrome_cleaner::String16EmbeddedNulls& value_name);

typedef base::RepeatingCallback<bool(
    const chrome_cleaner::String16EmbeddedNulls& key,
    const chrome_cleaner::String16EmbeddedNulls& value_name)>
    ShouldNormalizeRegistryValue;

bool DefaultShouldValueBeNormalized(
    const chrome_cleaner::String16EmbeddedNulls& key,
    const chrome_cleaner::String16EmbeddedNulls& value_name);

bool SandboxNtChangeRegistryValue(
    const chrome_cleaner::String16EmbeddedNulls& key,
    const chrome_cleaner::String16EmbeddedNulls& value_name,
    const chrome_cleaner::String16EmbeddedNulls& new_value,
    const ShouldNormalizeRegistryValue& should_normalize_callback);

bool SandboxDeleteService(const base::string16& name);

bool SandboxDeleteTask(const base::string16& name);

TerminateProcessResult SandboxTerminateProcess(uint32_t process_id);

}  // namespace chrome_cleaner_sandbox

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_SANDBOX_INTERFACE_H_
