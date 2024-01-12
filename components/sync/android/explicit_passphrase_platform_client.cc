// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/android/explicit_passphrase_platform_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/android/jni_headers/ExplicitPassphrasePlatformClient_jni.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

void SendExplicitPassphraseToJavaPlatformClient(
    const syncer::SyncService* sync_service) {
  std::unique_ptr<syncer::Nigori> nigori_key =
      sync_service->GetUserSettings()
          ->GetExplicitPassphraseDecryptionNigoriKey();
  if (!nigori_key) {
    return;
  }

  sync_pb::NigoriKey proto;
  proto.set_deprecated_name(nigori_key->GetKeyName());
  nigori_key->ExportKeys(proto.mutable_deprecated_user_key(),
                         proto.mutable_encryption_key(),
                         proto.mutable_mac_key());
  int32_t byte_size = proto.ByteSize();
  std::vector<uint8_t> bytes(byte_size);
  proto.SerializeToArray(bytes.data(), byte_size);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExplicitPassphrasePlatformClient_setExplicitDecryptionPassphrase(
      env, ConvertToJavaCoreAccountInfo(env, sync_service->GetAccountInfo()),
      base::android::ToJavaByteArray(env, bytes.data(), byte_size));
}

}  // namespace syncer
