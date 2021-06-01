// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_METRICS_H_
#define COMPONENTS_VARIATIONS_METRICS_H_

#include "build/build_config.h"

namespace variations {

#if defined(OS_ANDROID)
// The result of importing a seed during Android first run.
// Note: UMA histogram enum - don't re-order or remove entries.
enum class FirstRunSeedImportResult {
  SUCCESS,
  FAIL_NO_CALLBACK,
  FAIL_NO_FIRST_RUN_SEED,
  FAIL_STORE_FAILED,
  FAIL_INVALID_RESPONSE_DATE,
  ENUM_SIZE
};
#endif  // OS_ANDROID

// The result of attempting to load a variations seed on startup.
// Note: UMA histogram enum - don't re-order or remove entries.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.variations
enum class LoadSeedResult {
  SUCCESS,
  EMPTY,
  CORRUPT,
  INVALID_SIGNATURE,
  CORRUPT_BASE64,
  CORRUPT_PROTOBUF,
  CORRUPT_GZIP,
  LOAD_TIMED_OUT,
  LOAD_INTERRUPTED,
  LOAD_OTHER_FAILURE,
  ENUM_SIZE
};

// The result of attempting to store a variations seed received from the server.
// Note: UMA histogram enum - don't re-order or remove entries.
enum class StoreSeedResult {
  SUCCESS,
  FAILED_EMPTY,
  FAILED_PARSE,
  FAILED_SIGNATURE,
  FAILED_GZIP,
  DELTA_COUNT_OBSOLETE,
  FAILED_DELTA_READ_SEED,
  FAILED_DELTA_APPLY,
  FAILED_DELTA_STORE,
  FAILED_UNGZIP,
  FAILED_EMPTY_GZIP_CONTENTS,
  FAILED_UNSUPPORTED_SEED_FORMAT,
  // The following are not so much a result of the seed store, but rather
  // counting the types of seeds the SeedStore() function saw. Kept in the same
  // histogram for efficiency and convenience of comparing against the other
  // values.
  GZIP_DELTA_COUNT,
  NON_GZIP_DELTA_COUNT,
  GZIP_FULL_COUNT,
  NON_GZIP_FULL_COUNT,
  ENUM_SIZE
};

// The result of updating the date associated with an existing stored variations
// seed.
// Note: UMA histogram enum - don't re-order or remove entries.
enum class UpdateSeedDateResult {
  NO_OLD_DATE,
  NEW_DATE_IS_OLDER,
  SAME_DAY,
  NEW_DAY,
  ENUM_SIZE
};

// The result of verifying a variation seed's signature.
// Note: UMA histogram enum - don't re-order or remove entries.
enum class VerifySignatureResult {
  MISSING_SIGNATURE,
  DECODE_FAILED,
  INVALID_SIGNATURE,
  INVALID_SEED,
  VALID_SIGNATURE,
  ENUM_SIZE
};

#if defined(OS_ANDROID)
// Records the result of importing a seed during Android first run.
void RecordFirstRunSeedImportResult(FirstRunSeedImportResult result);
#endif  // OS_ANDROID

// Records the result of attempting to load the latest variations seed on
// startup.
void RecordLoadSeedResult(LoadSeedResult state);

// Records the result of attempting to load the safe variations seed on startup.
void RecordLoadSafeSeedResult(LoadSeedResult state);

// Records the result of attempting to store a variations seed received from the
// server.
void RecordStoreSeedResult(StoreSeedResult result);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_METRICS_H_
