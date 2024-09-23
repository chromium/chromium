// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data_provider_file.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/key_data_file_delegate.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_validator.h"

namespace metrics::structured {

KeyDataProviderFile::KeyDataProviderFile(const base::FilePath& file_path,
                                         base::TimeDelta write_delay)
    : file_path_(file_path), write_delay_(write_delay) {
  key_data_ = std::make_unique<KeyData>(std::make_unique<KeyDataFileDelegate>(
      file_path_, write_delay_,
      base::BindOnce(&KeyDataProviderFile::OnKeyReady,
                     weak_ptr_factory_.GetWeakPtr())));
}

KeyDataProviderFile::~KeyDataProviderFile() = default;

bool KeyDataProviderFile::IsReady() {
  return is_data_loaded_;
}

std::optional<uint64_t> KeyDataProviderFile::GetId(
    const std::string& project_name) {
  DCHECK(IsReady());

  // Validates the project. If valid, retrieve the metadata associated
  // with the event.
  const auto* project_validator =
      validator::Validators::Get()->GetProjectValidator(project_name);

  if (!project_validator) {
    return std::nullopt;
  }
  return key_data_->Id(project_validator->project_hash(),
                       base::Days(project_validator->key_rotation_period()));
}

std::optional<uint64_t> KeyDataProviderFile::GetSecondaryId(
    const std::string& project_name) {
  return std::nullopt;
}

KeyData* KeyDataProviderFile::GetKeyData(const std::string& project_name) {
  DCHECK(IsReady());
  return key_data_.get();
}

void KeyDataProviderFile::Purge() {
  if (IsReady()) {
    key_data_->Purge();
  }
}

void KeyDataProviderFile::OnKeyReady() {
  is_data_loaded_ = true;
  NotifyKeyReady();
}

}  // namespace metrics::structured
