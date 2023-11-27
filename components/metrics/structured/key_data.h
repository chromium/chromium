// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/persistent_proto.h"
#include "components/metrics/structured/storage.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics::structured {

class KeyDataTest;

// KeyData is the central class for managing keys and generating hashes for
// structured metrics.
//
// The class maintains one key and its rotation data for every project defined
// in /tools/metrics/structured/sync/structured.xml. This can be used to
// generate:
//  - an ID for the project with KeyData::Id.
//  - a hash of a given value for an event with KeyData::HmacMetric.
//
// KeyData performs key rotation. Every project is associated with a rotation
// period, which is 90 days unless specified in structured.xml. Keys are rotated
// with a resolution of one day. They are guaranteed not to be used for
// HmacMetric or UserProjectId for longer than their rotation period, except in
// cases of local clock changes.
//
// When first created, every project's key rotation date is selected uniformly
// so that there is an even distribution of rotations across users. This means
// that, for most users, the first rotation period will be shorter than the
// standard full rotation period for that project.
//
// Key storage is backed by a PersistentProto, stored at the path given to the
// constructor.
class KeyData {
 public:
  KeyData(const base::FilePath& path,
          const base::TimeDelta& save_delay,
          base::OnceCallback<void()> on_initialized);
  ~KeyData();

  KeyData(const KeyData&) = delete;
  KeyData& operator=(const KeyData&) = delete;

  // Returns a digest of |value| for |metric| in the context of
  // |project_name_hash|. Terminology: a metric is a (name, value) pair, and an
  // event is a bundle of metrics. Each event is associated with a project.
  //
  //  - |project_name_hash| is the uint64 name hash of a project.
  //  - |metric_name_hash| is the uint64 name hash of a metric.
  //  - |value| is the string value to hash.
  //
  // The result is the HMAC digest of the |value| salted with |metric|, using
  // the key for |project_name_hash|. That is:
  //
  //   HMAC_SHA256(key(project_name_hash), concat(value, hex(event),
  //   hex(metric)))
  //
  // Returns 0u in case of an error.
  uint64_t HmacMetric(uint64_t project_name_hash,
                      uint64_t metric_name_hash,
                      const std::string& value,
                      int key_rotation_period);

  // Returns an ID for this (user, |project_name_hash|) pair.
  // |project_name_hash| is the name of a project, represented by the first 8
  // bytes of the MD5 hash of its name defined in structured.xml.
  //
  // The derived ID is the first 8 bytes of SHA256(key(project_name_hash)).
  // Returns 0u in case of an error.
  //
  // This ID is intended as the only ID for the events of a particular
  // structured metrics project. However, events are uploaded from the device
  // alongside the UMA client ID, which is only removed after the event reaches
  // the server. This means events are associated with the client ID when
  // uploaded from the device. See the class comment of
  // StructuredMetricsProvider for more details.
  //
  // Default |key_rotation_period| is 90 days.
  uint64_t Id(uint64_t project_name_hash, int key_rotation_period);

  // Returns when the key for |project_name_hash| was last rotated, in days
  // since epoch. Returns nullopt if the key doesn't exist.
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash) const;

  // Return the age of the key for |project_name_hash| since the last rotation,
  // in weeks.
  absl::optional<int> GetKeyAgeInWeeks(uint64_t project_name_hash) const;

  // Clears all key data from memory and from disk. If this is called before the
  // underlying proto has been read from disk, the purge will be performed once
  // the read is complete.
  void Purge();

  // Returns whether this KeyData instance has finished reading from disk and is
  // ready to be used. If false, both Id and HmacMetric will return 0u.
  bool is_initialized() { return is_initialized_; }

 private:
  friend class KeyDataTest;

  void WriteNowForTest();

  void OnRead(ReadStatus status);

  void OnWrite(WriteStatus status);

  // Ensure that a valid key exists for |project|, and return it. Either returns
  // a string of size |kKeySize| or absl::nullopt, which indicates an error. If
  // a key doesn't exist OR if the key needs to be rotated, then a new key with
  // |key_rotation_period| will be created.
  absl::optional<std::string> ValidateAndGetKey(uint64_t project_name_hash,
                                                int key_rotation_period);

  // Regenerate |key|, also updating the |last_key_rotation| and
  // |key_rotation_period|. This triggers a save.
  void UpdateKey(KeyProto* key, int last_key_rotation, int key_rotation_period);

  // Storage for keys.
  std::unique_ptr<PersistentProto<KeyDataProto>> proto_;

  // Whether this instance has finished reading from disk.
  bool is_initialized_ = false;

  base::OnceCallback<void()> on_initialized_;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<KeyData> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_
