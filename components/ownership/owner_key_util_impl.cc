// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util_impl.h"

#include <keythi.h>
#include <limits>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "crypto/nss_key_util.h"

namespace ownership {

static const uint16_t kKeySizeInBits = 2048;

OwnerKeyUtilImpl::OwnerKeyUtilImpl(const base::FilePath& public_key_file)
    : public_key_file_(public_key_file) {}

OwnerKeyUtilImpl::~OwnerKeyUtilImpl() = default;

scoped_refptr<PublicKey> OwnerKeyUtilImpl::ImportPublicKey() {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64_t file_size;
  if (!base::GetFileSize(public_key_file_, &file_size)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Could not get size of " << public_key_file_.value();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return nullptr;
  }
  if (file_size > static_cast<int64_t>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << public_key_file_.value() << "is " << file_size
               << "bytes!!!  Too big!";
    return nullptr;
  }
  int32_t safe_file_size = static_cast<int32_t>(file_size);

  std::vector<uint8_t> key_data;
  key_data.resize(safe_file_size);

  if (safe_file_size == 0) {
    LOG(WARNING) << "Public key file is empty. This seems wrong.";
    return nullptr;
  }

  // Get the key data off of disk
  int data_read =
      base::ReadFile(public_key_file_, reinterpret_cast<char*>(key_data.data()),
                     safe_file_size);
  if (data_read != safe_file_size) {
    return nullptr;
  }

  return base::MakeRefCounted<ownership::PublicKey>(
      /*is_persisted=*/true, std::move(key_data));
}

crypto::ScopedSECKEYPrivateKey OwnerKeyUtilImpl::GenerateKeyPair(
    PK11SlotInfo* slot) {
  DCHECK(slot);

  PK11RSAGenParams param;
  param.keySizeInBits = kKeySizeInBits;
  param.pe = 65537L;
  SECKEYPublicKey* public_key_ptr = nullptr;

  crypto::ScopedSECKEYPrivateKey key(PK11_GenerateKeyPair(
      slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &param, &public_key_ptr,
      PR_TRUE /* permanent */, PR_TRUE /* sensitive */, nullptr));
  crypto::ScopedSECKEYPublicKey public_key(public_key_ptr);
  return key;
}

crypto::ScopedSECKEYPrivateKey OwnerKeyUtilImpl::FindPrivateKeyInSlot(
    const std::vector<uint8_t>& key,
    PK11SlotInfo* slot) {
  if (!slot)
    return nullptr;

  crypto::ScopedSECKEYPrivateKey private_key(
      crypto::FindNSSKeyFromPublicKeyInfoInSlot(key, slot));
  if (!private_key || SECKEY_GetPrivateKeyType(private_key.get()) != rsaKey)
    return nullptr;
  return private_key;
}

bool OwnerKeyUtilImpl::IsPublicKeyPresent() {
  return base::PathExists(public_key_file_);
}

}  // namespace ownership
