// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_KEY_DATA_FILE_DELEGATE_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_KEY_DATA_FILE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/persistent_proto.h"
#include "components/metrics/structured/lib/proto/key.pb.h"

namespace base {

class FilePath;
class TimeDelta;

}  // namespace base

namespace metrics::structured {

// File-backed KeyData::StorageDelegate implementation using PersistentProto,
// stored at the path given to the constructor that will persist any changes on
// |save_delay| cadence.
class KeyDataFileDelegate : public KeyData::StorageDelegate {
 public:
  // Stores a file at |path| that updates every |save_delay|. Once the keys have
  // been loaded from |path|, callback |on_initialized_callback| will be called.
  KeyDataFileDelegate(const base::FilePath& path,
                      base::TimeDelta save_delay,
                      base::OnceClosure on_initialized_callback);
  ~KeyDataFileDelegate() override;

  // KeyData::StorageDelegate:
  bool IsReady() const override;
  const KeyProto* GetKey(uint64_t project_name_hash) const override;
  void UpsertKey(uint64_t project_name_hash,
                 base::TimeDelta last_key_rotation,
                 base::TimeDelta key_rotation_period) override;
  void Purge() override;

 private:
  friend class KeyDataFileDelegateTest;

  // Flushes immediately for tests that need to read the file immediately.
  void WriteNowForTesting();

  // Callback made when |proto_| is initially read.
  void OnRead(ReadStatus status);

  // Callback made when there is a write to |proto_|.
  void OnWrite(WriteStatus status);

  // Whether this instance has finished reading from disk.
  bool is_initialized_ = false;

  // Callback made once |proto_| has been read and loaded into memory.
  base::OnceClosure on_initialized_callback_;

  // File-backed storage for keys.
  std::unique_ptr<PersistentProto<KeyDataProto>> proto_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<KeyDataFileDelegate> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_KEY_DATA_FILE_DELEGATE_H_
