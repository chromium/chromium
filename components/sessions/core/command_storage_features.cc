// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_features.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace sessions {

BASE_FEATURE(kEncryptSessionStorage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kEncryptSessionStorageStageParam,
                   &kEncryptSessionStorage,
                   "stage",
                   "");

using ::sessions::internal::kEncryptSessionStorageStageWriteBothReadOnlyClear;
using ::sessions::internal::
    kEncryptSessionStorageStageWriteBothReadPreferEncrypted;
using ::sessions::internal::
    kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted;

EncryptSessionStorageStage GetEncryptSessionStorageStage() {
  if (!base::FeatureList::IsEnabled(kEncryptSessionStorage)) {
    return EncryptSessionStorageStage::kClearOnly;
  }
  std::string param_str = kEncryptSessionStorageStageParam.Get();
  if (param_str.empty()) {
    return EncryptSessionStorageStage::kClearOnly;
  } else if (param_str == kEncryptSessionStorageStageWriteBothReadOnlyClear) {
    return EncryptSessionStorageStage::kWriteBothReadOnlyClear;
  } else if (param_str ==
             kEncryptSessionStorageStageWriteBothReadPreferEncrypted) {
    return EncryptSessionStorageStage::kWriteBothReadPreferEncrypted;
  } else if (param_str ==
             kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted) {
    return EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted;
  } else {
    VLOG(1) << "Unknown EncryptSessionStorageStage: " << param_str;
    return EncryptSessionStorageStage::kClearOnly;
  }
}

}  // namespace sessions
