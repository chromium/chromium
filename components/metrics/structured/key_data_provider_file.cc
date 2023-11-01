// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data_provider_file.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_validator.h"

namespace metrics::structured {

KeyDataProviderFile::KeyDataProviderFile(
    const base::FilePath& file_path,
    base::TimeDelta write_delay,
    base::OnceClosure on_key_ready_callback)
    : file_path_(file_path),
      write_delay_(write_delay),
      on_key_ready_callback_(std::move(on_key_ready_callback)) {
  key_data_ =
      std::make_unique<KeyData>(file_path_, write_delay_,
                                base::BindOnce(&KeyDataProviderFile::OnKeyReady,
                                               weak_ptr_factory_.GetWeakPtr()));
}

KeyDataProviderFile::~KeyDataProviderFile() = default;

bool KeyDataProviderFile::IsReady() {
  return is_data_loaded_;
}

void KeyDataProviderFile::OnKeyReady() {
  is_data_loaded_ = true;

  if (!on_key_ready_callback_.is_null()) {
    std::move(on_key_ready_callback_).Run();
  }
}

absl::optional<uint64_t> KeyDataProviderFile::GetId(
    const std::string& project_name) {
  DCHECK(IsReady());

  // Validates the project. If valid, retrieve the metadata associated
  // with the event.
  auto maybe_project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);

  if (!maybe_project_validator.has_value()) {
    return absl::nullopt;
  }
  const auto* project_validator = maybe_project_validator.value();
  return key_data_->Id(project_validator->project_hash(),
                       project_validator->key_rotation_period());
}

absl::optional<uint64_t> KeyDataProviderFile::GetSecondaryId(
    const std::string& project_name) {
  return absl::nullopt;
}

KeyData* KeyDataProviderFile::GetKeyData(const std::string& project_name) {
  DCHECK(IsReady());
  return key_data_.get();
}

// Unimplemented as this API will be removed.
void KeyDataProviderFile::InitializeDeviceKey(base::OnceClosure callback) {}

// Unimplemented as this API will be removed.
void KeyDataProviderFile::InitializeProfileKey(
    const base::FilePath& profile_path,
    base::OnceClosure callback) {}

KeyData* KeyDataProviderFile::GetDeviceKeyData() {
  // This API will be removed.
  return key_data_.get();
}

KeyData* KeyDataProviderFile::GetProfileKeyData() {
  // This API will be removed.
  return key_data_.get();
}

void KeyDataProviderFile::Purge() {
  if (IsReady()) {
    key_data_->Purge();
  }
}

bool KeyDataProviderFile::HasProfileKey() {
  // This API will be removed.
  return true;
}

bool KeyDataProviderFile::HasDeviceKey() {
  // This API will be removed.
  return true;
}

}  // namespace metrics::structured
