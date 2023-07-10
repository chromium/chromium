// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_cryptographer.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace syncer {

namespace {

const char kSeparator[] = "@@";

}  // namespace

// static
std::unique_ptr<FakeCryptographer> FakeCryptographer::FromSingleDefaultKey(
    const std::string& key_name) {
  auto cryptographer = std::make_unique<FakeCryptographer>();
  cryptographer->AddEncryptionKey(key_name);
  cryptographer->SelectDefaultEncryptionKey(key_name);
  return cryptographer;
}

FakeCryptographer::FakeCryptographer() = default;

FakeCryptographer::~FakeCryptographer() = default;

void FakeCryptographer::AddEncryptionKey(const std::string& key_name) {
  DCHECK(!key_name.empty());
  known_key_names_.insert(key_name);
}

void FakeCryptographer::SelectDefaultEncryptionKey(
    const std::string& key_name) {
  DCHECK(base::Contains(known_key_names_, key_name));
  default_key_name_ = key_name;
}

void FakeCryptographer::ClearDefaultEncryptionKey() {
  default_key_name_.clear();
}

bool FakeCryptographer::CanEncrypt() const {
  return !default_key_name_.empty();
}

bool FakeCryptographer::CanDecrypt(
    const sync_pb::EncryptedData& encrypted) const {
  return base::Contains(known_key_names_, encrypted.key_name());
}

std::string FakeCryptographer::GetDefaultEncryptionKeyName() const {
  return default_key_name_;
}

bool FakeCryptographer::EncryptString(const std::string& decrypted,
                                      sync_pb::EncryptedData* encrypted) const {
  if (default_key_name_.empty()) {
    return false;
  }
  *encrypted->mutable_key_name() = default_key_name_;
  *encrypted->mutable_blob() =
      base::StrCat({default_key_name_, kSeparator, decrypted});
  return true;
}

bool FakeCryptographer::DecryptToString(const sync_pb::EncryptedData& encrypted,
                                        std::string* decrypted) const {
  auto key_iter = base::ranges::find(known_key_names_, encrypted.key_name());
  if (key_iter == known_key_names_.end()) {
    return false;
  }

  std::string key_name_and_separator = base::StrCat({*key_iter, kSeparator});
  if (!base::StartsWith(encrypted.blob(), key_name_and_separator)) {
    // Corrupted blob.
    return false;
  }

  *decrypted =
      std::string(encrypted.blob().begin() + key_name_and_separator.size(),
                  encrypted.blob().end());
  return true;
}

const CrossUserSharingPublicPrivateKeyPair&
FakeCryptographer::GetCrossUserSharingKeyPairForTesting(
    uint32_t version) const {
  return cross_user_sharing_key_pair;
}

}  // namespace syncer
