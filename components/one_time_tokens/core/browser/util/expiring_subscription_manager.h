// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_MANAGER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace one_time_tokens {

namespace internal {

// This is an internal storage type for the subscription manager
// (non-template-ized parts).
struct ExpiringSubscriptionDataBase {
  ExpiringSubscriptionDataBase() = default;
  virtual ~ExpiringSubscriptionDataBase() = default;

  base::Time expiration;
};

// This is an internal storage type for the subscription manager.
template <typename Signature>
struct ExpiringSubscriptionData : public ExpiringSubscriptionDataBase {
  ExpiringSubscriptionData() = default;
  ~ExpiringSubscriptionData() override = default;

  base::RepeatingCallback<Signature> notification_callback;
};

}  // namespace internal

// The non-template-ized parts of `ExpiringSubscriptionManager`.
class ExpiringSubscriptionManagerBase {
 public:
  ExpiringSubscriptionManagerBase();
  ExpiringSubscriptionManagerBase(const ExpiringSubscriptionManagerBase&) =
      delete;
  ExpiringSubscriptionManagerBase& operator=(
      const ExpiringSubscriptionManagerBase&) = delete;
  ~ExpiringSubscriptionManagerBase();

  // Returns true if the expiration is active (not expired).
  [[nodiscard]] bool Exists(const ExpiringSubscriptionHandle& handle) const;

  // Returns the expiration time to `handle`. Returns a null base::Time in case
  // the subscription does not exist.
  [[nodiscard]] base::Time GetExpirationTime(
      const ExpiringSubscriptionHandle& handle) const;

  // Updates the expiration date. Does nothing if the subscription is unknown.
  void SetExpirationTime(const ExpiringSubscriptionHandle& handle,
                         base::Time new_expiration);

  // Cancels the subscription identified by `handle`. No further callbacks will
  // happen and the subscription manager will immediately lose all knowledge
  // about the subscription. Does nothing if the handle is unknown
  void Cancel(const ExpiringSubscriptionHandle& handle);

  size_t GetNumberSubscribers() const;

 protected:
  // Determines the list of subscriptions that are expired and cancels those
  // subscriptions.
  //
  // Note: Expiration checks are based on the time captured at the start of the
  // expiration processing cycle. Long-running callbacks will not affect which
  // subscriptions are considered expired within that single cycle.
  void ProcessExpirations();

  // Configures `next_expiration_timer_` to call `ProcessExpirations` when
  // the soonest upcoming subscription times out. Cancels the timer if no
  // subscription remains.
  void UpdateNextExpirationTimer();

  // Returns the time when the next subscription expires.
  std::optional<base::Time> GetNextExpirationTime();

  base::flat_map<ExpiringSubscriptionHandle,
                 std::unique_ptr<internal::ExpiringSubscriptionDataBase>>
      subscriptions_;

  base::OneShotTimer next_expiration_timer_;
  std::optional<base::Time> next_expected_expiration_;

  base::WeakPtrFactory<ExpiringSubscriptionManagerBase> weak_ptr_factory_{this};
};

// An ExpiringSubscriptionManager allows subscribers to be notified about events
// until a certain expiration time passed. Up to this time all notifications
// will be forwarded. When the expiration happens, the subscriber is informed
// about this.
//
// The order in which subscribers are notified about events is undefined.
//
// `Signature` describes the signature of a callback function.
//
// Note: This ExpiringSubscriptionManager is not tuned to deal with hundreds
// of subscriptions at a time.
template <typename Signature>
class ExpiringSubscriptionManager : public ExpiringSubscriptionManagerBase {
 public:
  ExpiringSubscriptionManager() = default;
  ExpiringSubscriptionManager(const ExpiringSubscriptionManager&) = delete;
  ExpiringSubscriptionManager& operator=(const ExpiringSubscriptionManager&) =
      delete;
  ~ExpiringSubscriptionManager() = default;

  // Creates a subscription that is valid until
  // - the expiration passed, or
  // - until `Cancel()` is called or
  // - until the ExpiringSubscriptionManager is destroyed.
  //
  // `callback` should be safe to call during this time (it may be protected via
  // a WeakPtr binding). It is called every time `Notify()` is called while the
  // subscription did not not expire.
  //
  // When the returned `ExpiringSubscription` is deallocated, the subscription
  // is canceled without further callbacks or notifications.
  [[nodiscard]] ExpiringSubscription Subscribe(
      base::Time expiration_time,
      base::RepeatingCallback<Signature> callback) {
    ExpiringSubscriptionHandle handle(base::Uuid::GenerateRandomV4());
    auto subscription_data =
        std::make_unique<internal::ExpiringSubscriptionData<Signature>>();
    subscription_data->expiration = expiration_time;
    subscription_data->notification_callback = std::move(callback);
    subscriptions_[handle] = std::move(subscription_data);
    UpdateNextExpirationTimer();
    return ExpiringSubscription(std::move(handle),
                                weak_ptr_factory_.GetWeakPtr());
  }

  // Calls the notification callback on all subscribers with the handle of the
  // subscription followed by `args`.
  // This method is safe against re-entrancy. A notification callback is
  // permitted to add, cancel, or modify subscriptions.
  template <typename... RunArgs>
  void Notify(const RunArgs&... args) {
    // Copy all handles because a callback may modify the subscriptions_.
    std::vector<ExpiringSubscriptionHandle> all_handles;
    all_handles.reserve(subscriptions_.size());
    for (auto& [handle, _] : subscriptions_) {
      all_handles.push_back(handle);
    }

    // Execute the notifications.
    for (const ExpiringSubscriptionHandle& handle : all_handles) {
      if (auto iter = subscriptions_.find(handle);
          iter != subscriptions_.end()) {
        auto& subscription = *iter->second;
        static_cast<internal::ExpiringSubscriptionData<Signature>&>(
            subscription)
            .notification_callback.Run(args...);
      }
    }
  }
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_SUBSCRIPTION_MANAGER_H_
