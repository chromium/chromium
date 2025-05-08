// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_data_storage.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace {

// Size of a PRT when TLS serialized, before base64 encoding.
constexpr size_t kPRTSize = 79;
constexpr size_t kPRTPointSize = 33;
constexpr size_t kEpochIdSize = 8;

base::TimeDelta GetNextTokenRequestDelta(base::Time next_epoch_start_time,
                                         base::Time expiration_time) {
  if (next_epoch_start_time < base::Time::Now()) {
    // Either client time is wrong or PRT issuer server returned wrong
    // next_epoch_start, most likely client time. Schedule next request
    // in three hours.
    return base::Hours(3);
  }

  // Schedule next request at a random time between next_epoch_start_time and
  // expiration_time - 10 minutes.
  base::Time now = base::Time::Now();
  base::TimeDelta delta_to_next_epoch = next_epoch_start_time - now;
  base::TimeDelta delta_to_expiration =
      expiration_time - base::Minutes(10) - now;
  if (delta_to_expiration <= delta_to_next_epoch) {
    // If expiration_time (minus 10 minutes) is before next_epoch_start_time,
    // base::RandTimeDelta will fail. This should not normally happen as the PRT
    // issuer server should set a larger time gap between next_epoch_start_time
    // and expiration_time. But if it does, fallback to scheduling the next
    // request at next_epoch_start_time.
    return delta_to_next_epoch;
  }
  return base::RandTimeDelta(delta_to_next_epoch, delta_to_expiration);
}

}  // namespace

namespace ip_protection {

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("ProbabilisticRevealTokens");

std::optional<base::FilePath> GetDBPath(
    std::optional<base::FilePath> data_directory) {
  if (!data_directory.has_value()) {
    // The data directory will be nullopt if the
    // `StoreProbabilisticRevealTokens` switch is disabled. In this case, we
    // pass nullopt to the storage class. This will prevent tokens from being
    // written to disk even if StoreTokenOutcome() is called.
    return std::nullopt;
  }
  return data_directory->Append(kDatabaseName);
}

}  // namespace

IpProtectionProbabilisticRevealTokenManager::
    IpProtectionProbabilisticRevealTokenManager(
        std::unique_ptr<IpProtectionProbabilisticRevealTokenFetcher> fetcher,
        std::optional<base::FilePath> data_directory)
    : fetcher_(std::move(fetcher)),
      expiration_(base::Time::UnixEpoch()),
      storage_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               GetDBPath(data_directory)) {
  DCHECK(fetcher_);
}

IpProtectionProbabilisticRevealTokenManager::
    ~IpProtectionProbabilisticRevealTokenManager() {
  refetch_timer_.Stop();
}

void IpProtectionProbabilisticRevealTokenManager::RequestTokens() {
  if (!fetcher_) {
    // This should not happen in theory, caller should not hand
    // manager a null fetcher.
    return;
  }
  token_fetch_start_time_ = base::TimeTicks::Now();
  fetcher_->TryGetProbabilisticRevealTokens(base::BindOnce(
      &IpProtectionProbabilisticRevealTokenManager::OnTryGetTokens,
      weak_ptr_factory_.GetWeakPtr()));
}

void IpProtectionProbabilisticRevealTokenManager::OnTryGetTokens(
    std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
    TryGetProbabilisticRevealTokensResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Telemetry().GetProbabilisticRevealTokensComplete(
      result.status, base::TimeTicks::Now() - token_fetch_start_time_);

  if (result.try_again_after.has_value()) {
    // Network error when fetching, re-try at the time fetcher asked. Fetcher
    // implements exponential backoff.
    refetch_timer_.Start(
        FROM_HERE, result.try_again_after.value() - base::Time::Now(), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  } else if (result.status != TryGetProbabilisticRevealTokensStatus::kSuccess) {
    // Response returned from server, however it is invalid.
    // This might happen if the PRT issuer server is configured wrong.
    // Fetcher did not specify when to retry. Retry in an hour.
    refetch_timer_.Start(
        FROM_HERE, base::Hours(1), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  }
  DCHECK(outcome.has_value());
  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          outcome.value().public_key, outcome.value().tokens);
  if (!maybe_crypter.has_value()) {
    // Might happen if PRT issuer is misconfigured and public_key or tokens do
    // not belong to the group. Return without clearing existing tokens, which
    // might be still valid for current epoch. Retry in an hour.
    refetch_timer_.Start(
        FROM_HERE, base::Hours(1), this,
        &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
    return;
  }
  crypter_ = std::move(maybe_crypter.value());
  token_map_.clear();
  expiration_ = base::Time::FromMillisecondsSinceUnixEpoch(
      base::Seconds(outcome.value().expiration_time_seconds).InMilliseconds());
  num_tokens_with_signal_ = outcome.value().num_tokens_with_signal;
  epoch_id_ = outcome.value().epoch_id;

  StoreTokenOutcomeIfEnabled(*outcome);

  base::Time next_epoch_start_time = base::Time::FromMillisecondsSinceUnixEpoch(
      base::Seconds(outcome.value().next_epoch_start_time_seconds)
          .InMilliseconds());
  base::TimeDelta next_request_delta =
      GetNextTokenRequestDelta(next_epoch_start_time, expiration_);
  refetch_timer_.Start(
      FROM_HERE, next_request_delta, this,
      &IpProtectionProbabilisticRevealTokenManager::RequestTokens);
}

bool IpProtectionProbabilisticRevealTokenManager::AreTokensExpired() const {
  return (base::Time::Now() >= expiration_);
}

void IpProtectionProbabilisticRevealTokenManager::ClearTokens() {
  token_map_.clear();
  if (crypter_) {
    crypter_->ClearTokens();
  }
}

void IpProtectionProbabilisticRevealTokenManager::ClearTokensIfExpired() {
  if (AreTokensExpired()) {
    ClearTokens();
  }
}

bool IpProtectionProbabilisticRevealTokenManager::IsTokenAvailable() {
  ClearTokensIfExpired();
  return crypter_ && crypter_->IsTokenAvailable();
}

std::optional<std::string>
IpProtectionProbabilisticRevealTokenManager::GetToken(
    const std::string& top_level,
    const std::string& third_party) {
  ClearTokensIfExpired();

  bool is_token_available = IsTokenAvailable();
  Telemetry().IsProbabilisticRevealTokenAvailable(is_initial_get_token_call_,
                                                  is_token_available);
  is_initial_get_token_call_ = false;

  if (!is_token_available) {
    return std::nullopt;
  }
  // Manager has tokens, crypter_ is not null from here on and
  // crypter_->NumTokens() > 0 holds.
  auto outer_iterator = token_map_.find(top_level);
  if (outer_iterator != token_map_.end()) {
    // First party already has an associated token.
    std::size_t token_index = outer_iterator->second.first;
    std::map<std::string, ProbabilisticRevealToken>& inner_map =
        outer_iterator->second.second;

    const auto inner_iterator = inner_map.find(third_party);
    if (inner_iterator != inner_map.end()) {
      // The pair already has a randomized token.
      return SerializePrt(inner_iterator->second);
    }

    // Seeing this third party for the first time in this top level.
    // Randomize top level's token and return.
    base::expected<ProbabilisticRevealToken, absl::Status>
        maybe_randomized_token = crypter_->Randomize(token_index);
    if (!maybe_randomized_token.has_value()) {
      // Should not happen in theory, might happen with corrupted crypter.
      return std::nullopt;
    }
    inner_map[third_party] = std::move(maybe_randomized_token.value());
    return SerializePrt(inner_map[third_party]);
  }
  // Seeing first party for the first time.
  std::size_t token_selected = base::RandGenerator(crypter_->NumTokens());
  base::expected<ProbabilisticRevealToken, absl::Status>
      maybe_randomized_token = crypter_->Randomize(token_selected);
  if (!maybe_randomized_token.has_value()) {
    // Should not happen in theory, might happen with corrupted crypter.
    return std::nullopt;
  }
  token_map_[top_level] = {token_selected,
                           {{third_party, maybe_randomized_token.value()}}};
  return SerializePrt(std::move(maybe_randomized_token).value());
}

void IpProtectionProbabilisticRevealTokenManager::StoreTokenOutcomeIfEnabled(
    TryGetProbabilisticRevealTokensOutcome outcome) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          network::switches::kStoreProbabilisticRevealTokens)) {
    storage_
        .AsyncCall(
            &IpProtectionProbabilisticRevealTokenDataStorage::StoreTokenOutcome)
        .WithArgs(outcome);
  }
}

/*
Serialize the following struct given in TLS presentation language (rfc8446
section-3). Size of u and e depends on the version and only possible version
value is 1 for now. Only possible size for u and e is 33. Returns null in case
of failure.

struct {
  uint8 version;
  opaque u<0..2^16-1>;
  opaque e<0..2^16-1>;
  opaque epoch_id[8];
} tlsPRT;

Once serialized, output bytes will be as follows.

[1 byte for version |
 2 bytes for u size | 33 bytes for u |
 2 bytes for e size | 33 bytes for e |
 8 bytes for epoch_id]
*/
std::optional<std::string>
IpProtectionProbabilisticRevealTokenManager::SerializePrt(
    ProbabilisticRevealToken token) {
  if (token.version != 1 || token.u.size() != token.e.size() ||
      token.u.size() != kPRTPointSize || epoch_id_.size() != kEpochIdSize) {
    return std::nullopt;
  }
  bssl::ScopedCBB cbb;
  std::string prt(kPRTSize, 0);
  size_t cbb_size = 0;
  // CBB doc says CBB_init_fixed will not fail.
  CHECK(CBB_init_fixed(cbb.get(), reinterpret_cast<uint8_t*>(prt.data()),
                       kPRTSize));
  if (!CBB_add_u8(cbb.get(), token.version) ||
      !CBB_add_u16(cbb.get(), token.u.size()) ||
      !CBB_add_bytes(cbb.get(),
                     reinterpret_cast<const uint8_t*>(token.u.data()),
                     token.u.size()) ||
      !CBB_add_u16(cbb.get(), token.e.size()) ||
      !CBB_add_bytes(cbb.get(),
                     reinterpret_cast<const uint8_t*>(token.e.data()),
                     token.e.size()) ||
      !CBB_add_bytes(cbb.get(),
                     reinterpret_cast<const uint8_t*>(epoch_id_.data()),
                     epoch_id_.size()) ||
      !CBB_finish(cbb.get(), nullptr, &cbb_size)) {
    return std::nullopt;
  }
  CHECK_EQ(cbb_size, kPRTSize);
  return prt;
}

}  // namespace ip_protection
