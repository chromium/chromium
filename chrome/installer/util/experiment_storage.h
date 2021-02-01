// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_EXPERIMENT_STORAGE_H_
#define CHROME_INSTALLER_UTIL_EXPERIMENT_STORAGE_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"

namespace installer {

class Experiment;
struct ExperimentMetrics;

// Manages the storage of experiment state on the machine.
//
// Participation is a per-install property evaluated one time to determine which
// study the client participates in. It is stored in the install's ClientState
// key.
//
// ExperimentMetrics are stored in a per-install experiment_label, while the
// fine-grained Experiment data are stored in a per-user key in Chrome's
// ClientState or ClientStateMedium key. For system-level installs, metrics are
// reported for only a single user on the machine, so only that one user will
// participate in the experiment.
//
// Owing to this "metrics are per-install" property in the face of potentially
// multiple users running the same install, ExperimentMetrics are considered the
// source of truth for an install's participation in the experiment. For any
// given state expressed in the experiment_label holding these metrics, there
// should be matching Experiment data for precisely one user.
//
// As state is written across multiple locations in the registry, a global
// mutex is used for all reads and writes to ensure consistent state.
class ExperimentStorage {
 public:
  // An identifier of which study the install participates in.
  enum Study : uint32_t {
    kNoStudySelected = 0,
    kStudyOne = 1,
    kStudyTwo = 2,
  };

  // Grants the holder exclusive access to the data in the registry. Consumers
  // are expected to not hold an instance across any blocking operations.
  class Lock {
   public:
    ~Lock();

    // Reads the participation state for the install. Returns false in case of
    // error. |participation| is set to kNotEvaluated if no value is present;
    // otherwise, it is set to the value found.
    bool ReadParticipation(Study* participation);

    // Writes the participation state for the install. Returns false if the
    // write failed.
    bool WriteParticipation(Study participation);

    // Loads the experiment metrics and data from the registry. Returns false if
    // the state in the registry corresponds to a different user or could not be
    // read.
    bool LoadExperiment(Experiment* experiment);

    // Stores the experiment metrics and data in |experiment| into the registry.
    bool StoreExperiment(const Experiment& experiment);

    // Loads per-install experiment metrics into |metrics|. Returns true if a
    // value was read or if none was found, in which case |metrics| is set to
    // the uninitialized state. Returns false in case of any error reading or
    // parsing the metrics.
    bool LoadMetrics(ExperimentMetrics* metrics);

    // Stores |metrics| in the per-install experiment_label.
    bool StoreMetrics(const ExperimentMetrics& metrics);

   private:
    friend ExperimentStorage;

    explicit Lock(ExperimentStorage* storage);

    ExperimentStorage* storage_;

    DISALLOW_COPY_AND_ASSIGN(Lock);
  };

  ExperimentStorage();
  ~ExperimentStorage();

  // Returns exclusive access to the experiment storage. The underlying
  // ExperimentStorage instance must not be deleted while the Lock* returned
  // here is still in use.
  std::unique_ptr<Lock> AcquireLock();

 private:
  FRIEND_TEST_ALL_PREFIXES(ExperimentStorageTest, TestEncodeDecodeMetrics);
  FRIEND_TEST_ALL_PREFIXES(ExperimentStorageTest, TestEncodeDecodeForMin);
  FRIEND_TEST_ALL_PREFIXES(ExperimentStorageTest, TestEncodeDecodeForMax);
  FRIEND_TEST_ALL_PREFIXES(ExperimentStorageTest, TestLoadStoreMetrics);
  FRIEND_TEST_ALL_PREFIXES(ExperimentStorageTest, TestLoadStoreExperiment);

  // Reads |bit_length| bits ending at |low_bit| of a 64 bit unsigned int into
  // an int.
  static int ReadUint64Bits(uint64_t source, int bit_length, int low_bit);

  // Sets the last |bit_length| bits of |value| into |target| at bit position
  // ending at low_bit.
  static void SetUint64Bits(int value,
                            int bit_length,
                            int low_bit,
                            uint64_t* target);

  // Decodes |encoded_metrics| into |metrics|, return true on success. Returns
  // false if the encoding is malformed.
  static bool DecodeMetrics(base::StringPiece16 encoded_metrics,
                            ExperimentMetrics* metrics);

  // Returns the encoded form of |metrics|.
  static base::string16 EncodeMetrics(const ExperimentMetrics& metrics);

  // Loads the per-install experiment metrics into |metrics|. Returns true if a
  // value was read or if none was found, in which case |metrics| is set to the
  // uninitialized state. Returns false in case of any error reading or parsing
  // the metrics.
  bool LoadMetricsUnsafe(ExperimentMetrics* metrics);

  // StoreMetrics without acquiring the mutex.
  bool StoreMetricsUnsafe(const ExperimentMetrics& metrics);

  // Loads the experiment state for the current user's Retention key into
  // |experiment|. Returns true if all values are read. Returns false otherwise,
  // in which case |experiment| is left in an undefined state.
  bool LoadStateUnsafe(Experiment* experiment);

  // Stores |experiment| in the current user's Retention key.
  bool StoreStateUnsafe(const Experiment& experiment);

  // A global mutex with a distinct name for the current installation.
  base::win::ScopedHandle mutex_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentStorage);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_EXPERIMENT_STORAGE_H_
