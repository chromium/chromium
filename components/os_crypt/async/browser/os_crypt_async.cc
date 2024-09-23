// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
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

  base::ranges::sort(input_providers, [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

#if DCHECK_IS_ON()
  for (auto it = input_providers.cbegin(); it != input_providers.cend() - 1;
       ++it) {
    DCHECK_NE(it->first, (it + 1)->first)
        << "Cannot have two providers with same precedence.";
  }
#endif  // DCHECK_IS_ON()

  base::ranges::transform(std::make_move_iterator(input_providers.begin()),
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
    encryptor_instance_ = base::WrapUnique<Encryptor>(new Encryptor());
    is_initialized_ = true;
  }
}

OSCryptAsync::~OSCryptAsync() = default;

// CallbackHelper is needed so the sequence checker member can be accessed in
// the callback, which it can't from a lambda without breaking the
// sequence_checker abstraction.
void OSCryptAsync::CallbackHelper(InitCallback callback,
                                  Encryptor::Option option) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(encryptor_instance_->Clone(option), /*result=*/true);
}

void OSCryptAsync::HandleKey(ProviderIterator current,
                             const std::string& tag,
                             std::optional<Encryptor::Key> key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (key && !tag.empty()) {
#if DCHECK_IS_ON()
    for (const auto& [key_name, key_value] : key_ring_) {
      // Check for overlapping names. Two providers called TEST and TEST2 are
      // likely incorrectly named, since TEST might try to decrypt TEST2's data.
      if (tag.size() > key_name.size()) {
        DCHECK(!std::equal(key_name.begin(), key_name.end(), tag.begin()))
            << "Tags must not overlap.";
      } else {
        DCHECK(!std::equal(tag.begin(), tag.end(), key_name.begin()))
            << "Tags must not overlap.";
      }
    }
#endif  // DCHECK_IS_ON()
    key->is_os_crypt_sync_compatible_ =
        ((*current)->IsCompatibleWithOsCryptSync());
    key_ring_.emplace(tag, std::move(*key));
    if ((*current)->UseForEncryption()) {
      provider_for_encryption_ = tag;
    }
  } else {
    // TODO(crbug.com/40241934): Return errors back via a callback.
    DVLOG(1) << "Provider " << tag << " failed to return a key.";
  }

  if (++current == providers_.end()) {
    encryptor_instance_ = base::WrapUnique<Encryptor>(
        new Encryptor(std::move(key_ring_), provider_for_encryption_));
    callbacks_.Notify();
    is_initialized_ = true;
    is_initializing_ = false;
    return;
  }

  (*current)->GetKey(base::BindOnce(&OSCryptAsync::HandleKey,
                                    weak_factory_.GetWeakPtr(), current));
}

base::CallbackListSubscription OSCryptAsync::GetInstance(
    InitCallback callback) {
  return GetInstance(std::move(callback), Encryptor::Option::kNone);
}

base::CallbackListSubscription OSCryptAsync::GetInstance(
    InitCallback callback,
    Encryptor::Option option) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_initialized_) {
    DCHECK(!is_initializing_);
    std::move(callback).Run(encryptor_instance_->Clone(option),
                            /*result=*/true);
    return base::CallbackListSubscription();
  }

  auto subscription = callbacks_.Add(
      base::BindOnce(&OSCryptAsync::CallbackHelper, weak_factory_.GetWeakPtr(),
                     std::move(callback), option));

  if (is_initializing_) {
    return subscription;
  }

  DCHECK(key_ring_.empty());
  is_initializing_ = true;

  const ProviderIterator start = providers_.cbegin();

  (*start)->GetKey(base::BindOnce(&OSCryptAsync::HandleKey,
                                  weak_factory_.GetWeakPtr(), start));

  return subscription;
}

}  // namespace os_crypt_async
