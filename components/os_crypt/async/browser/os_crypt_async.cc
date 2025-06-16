// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

namespace {

// Sorts `input_providers` by the `Precedence`, from lowest (beginning of
// vector) to highest (at the end), then returns a new vector that contains just
// the KeyProvider unique_ptrs in that new order.
std::vector<std::unique_ptr<KeyProvider>> SortProviders(
    std::vector<std::pair<OSCryptAsync::Precedence,
                          std::unique_ptr<KeyProvider>>> input_providers) {
  std::vector<std::unique_ptr<KeyProvider>> providers;
  if (input_providers.empty()) {
    return providers;
  }

  std::ranges::sort(input_providers, [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  for (auto it = input_providers.cbegin(); it != input_providers.cend() - 1;
       ++it) {
    CHECK_NE(it->first, (it + 1)->first)
        << "Cannot have two providers with same precedence.";
  }

  std::ranges::transform(std::make_move_iterator(input_providers.begin()),
                         std::make_move_iterator(input_providers.end()),
                         std::back_inserter(providers), [](auto provider) {
                           return std::move(provider.second);
                         });

  return providers;
}

}  // namespace

OSCryptAsync::OSCryptAsync(
    std::vector<std::pair<Precedence, std::unique_ptr<KeyProvider>>> providers)
    : providers_(SortProviders(std::move(providers))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (providers_.empty()) {
    SetEncryptorInstance(Encryptor());
  }
}

OSCryptAsync::~OSCryptAsync() = default;

// CallbackHelper is needed so the sequence checker member can be accessed in
// the callback, which it can't from a lambda without breaking the
// sequence_checker abstraction.
void OSCryptAsync::CallbackHelper(InitCallback callback,
                                  Encryptor::Option option) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(encryptor_instance_->Clone(option));
}

void OSCryptAsync::HandleKey(
    ProviderIterator current,
    const std::string& tag,
    base::expected<Encryptor::Key, KeyProvider::KeyError> key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!tag.empty()) << "Tag cannot be empty.";
  for (const auto& [key_name, key_value] : key_ring_) {
    // Check for overlapping names. Two providers called TEST and TEST2 are
    // likely incorrectly named, since TEST might try to decrypt TEST2's data.
    if (tag.size() > key_name.size()) {
      CHECK(!std::equal(key_name.begin(), key_name.end(), tag.begin()))
          << "Tags must not overlap.";
    } else {
      CHECK(!std::equal(tag.begin(), tag.end(), key_name.begin()))
          << "Tags must not overlap.";
    }
  }

  if (key.has_value()) {
    key_ring_.emplace(tag, std::move(*key));
    if ((*current)->UseForEncryption()) {
      provider_for_encryption_ = tag;
      if ((*current)->IsCompatibleWithOsCryptSync()) {
        provider_for_os_crypt_sync_compatible_encryption_ = tag;
      }
    }
  } else {
    switch (key.error()) {
      case KeyProvider::KeyError::kPermanentlyUnavailable:
        ++number_of_failing_key_providers_;
        DVLOG(1) << "Provider " << tag << " failed to return a key.";
        break;
      case KeyProvider::KeyError::kTemporarilyUnavailable:
        key_ring_.emplace(tag, std::nullopt);
        break;
    }
  }

  if (++current == providers_.end()) {
    SetEncryptorInstance(
        Encryptor(std::move(key_ring_), provider_for_encryption_,
                  provider_for_os_crypt_sync_compatible_encryption_));
    for (auto& callback : callbacks_) {
      std::move(callback).Run();
    }
    is_initializing_ = false;
    return;
  }

  (*current)->GetKey(base::BindOnce(&OSCryptAsync::HandleKey,
                                    weak_factory_.GetWeakPtr(), current));
}

void OSCryptAsync::SetEncryptorInstance(Encryptor encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_initialized_);
  is_initialized_ = true;
  encryptor_instance_ = std::make_unique<Encryptor>(std::move(encryptor));
  size_t available_keys = 0;
  size_t unavailable_keys = 0;
  for (const auto& key : encryptor_instance_->keys_) {
    if (key.second) {
      ++available_keys;
    } else {
      ++unavailable_keys;
    }
  }
  base::UmaHistogramCounts100(
      "OSCrypt.EncryptorKeyCount",
      number_of_failing_key_providers_ + available_keys + unavailable_keys);
  base::UmaHistogramCounts100("OSCrypt.EncryptorKeyCount.Available",
                              available_keys);
  base::UmaHistogramCounts100(
      "OSCrypt.EncryptorKeyCount.TemporarilyUnavailable", unavailable_keys);
  base::UmaHistogramCounts100(
      "OSCrypt.EncryptorKeyCount.PermanentlyUnavailable",
      number_of_failing_key_providers_);
}

void OSCryptAsync::GetInstance(InitCallback callback) {
  GetInstance(std::move(callback), Encryptor::Option::kNone);
}

void OSCryptAsync::GetInstance(InitCallback callback,
                               Encryptor::Option option) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_initialized_) {
    CHECK(!is_initializing_);
    std::move(callback).Run(encryptor_instance_->Clone(option));
    return;
  }

  callbacks_.emplace_back(base::BindOnce(&OSCryptAsync::CallbackHelper,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(callback), option));

  if (is_initializing_) {
    return;
  }

  CHECK(key_ring_.empty());
  is_initializing_ = true;

  const ProviderIterator start = providers_.cbegin();

  (*start)->GetKey(base::BindOnce(&OSCryptAsync::HandleKey,
                                  weak_factory_.GetWeakPtr(), start));
}

}  // namespace os_crypt_async
