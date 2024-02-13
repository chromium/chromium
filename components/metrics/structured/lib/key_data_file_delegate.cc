// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/key_data_file_delegate.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "components/metrics/structured/lib/key_util.h"
#include "components/metrics/structured/lib/persistent_proto.h"
#include "components/metrics/structured/lib/proto/key.pb.h"

namespace metrics::structured {

KeyDataFileDelegate::KeyDataFileDelegate(
    const base::FilePath& path,
    base::TimeDelta save_delay,
    base::OnceClosure on_initialized_callback)
    : on_initialized_callback_(std::move(on_initialized_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proto_ = std::make_unique<PersistentProto<KeyDataProto>>(
      path, save_delay,
      base::BindOnce(&KeyDataFileDelegate::OnRead, weak_factory_.GetWeakPtr()),
      base::BindRepeating(&KeyDataFileDelegate::OnWrite,
                          weak_factory_.GetWeakPtr()));
}

KeyDataFileDelegate::~KeyDataFileDelegate() = default;

bool KeyDataFileDelegate::IsReady() const {
  return is_initialized_;
}

const KeyProto* KeyDataFileDelegate::GetKey(uint64_t project_name_hash) const {
  const auto& keys = proto_.get()->get()->keys();
  auto it = keys.find(project_name_hash);
  if (it != keys.end()) {
    return &it->second;
  }
  return nullptr;
}

void KeyDataFileDelegate::UpsertKey(uint64_t project_name_hash,
                                    base::TimeDelta last_key_rotation,
                                    base::TimeDelta key_rotation_period) {
  KeyProto& key = (*(proto_.get()->get()->mutable_keys()))[project_name_hash];
  key.set_key(util::GenerateNewKey());
  key.set_last_rotation(last_key_rotation.InDays());
  key.set_rotation_period(key_rotation_period.InDays());
  proto_->QueueWrite();
}

void KeyDataFileDelegate::Purge() {
  proto_->Purge();
}

void KeyDataFileDelegate::OnRead(ReadStatus status) {
  is_initialized_ = true;
  switch (status) {
    case ReadStatus::kOk:
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kKeyReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kKeyParseError);
      break;
  }

  std::move(on_initialized_callback_).Run();
}

void KeyDataFileDelegate::OnWrite(WriteStatus status) {
  switch (status) {
    case WriteStatus::kOk:
      break;
    case WriteStatus::kWriteError:
      LogInternalError(StructuredMetricsError::kKeyWriteError);
      break;
    case WriteStatus::kSerializationError:
      LogInternalError(StructuredMetricsError::kKeySerializationError);
      break;
  }
}

void KeyDataFileDelegate::WriteNowForTesting() {
  proto_.get()->StartWriteForTesting();  // IN-TEST
}

}  // namespace metrics::structured
