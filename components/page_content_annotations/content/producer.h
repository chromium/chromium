// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PRODUCER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PRODUCER_H_

#include <stddef.h>

#include <utility>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {

template <typename T>
class SubscriberRegistry;

// Callback invoked by `SubscriberRegistry<T>::Notify()` when a `Producer<T>`
// produces a per-page artifact of type `T` for `page`. Callers may rely on
// this being a `base::RepeatingCallback` and use standard callback utilities.
template <typename T>
using ConsumerCallback =
    base::RepeatingCallback<void(content::Page& page, const T& payload)>;

// Interface for a pipeline stage or service that asynchronously produces
// per-page artifacts of type `T` (for example, extracted `PageContent` or
// computed `std::vector<PassageEmbedding>`). Active subscriptions also express
// downstream demand to the producer, allowing it to avoid performing expensive
// extraction or model execution when no consumers are subscribed.
template <typename T>
class Producer {
 public:
  Producer() = default;
  Producer(const Producer&) = delete;
  Producer& operator=(const Producer&) = delete;
  virtual ~Producer() = default;

  virtual base::CallbackListSubscription Subscribe(
      ConsumerCallback<T> callback) = 0;

 protected:
  friend class SubscriberRegistry<T>;

  // Invoked synchronously by `SubscriberRegistry<T>` whenever a subscriber is
  // added or removed so the producer can re-evaluate downstream demand and
  // start or stop upstream subscriptions or background work accordingly.
  virtual void OnDemandChanged() = 0;
};

// Component owned by a `Producer<T>` implementation to manage its subscribers,
// track downstream demand, and broadcast produced artifacts.
template <typename T>
class SubscriberRegistry {
 public:
  explicit SubscriberRegistry(Producer<T>& producer) : producer_(producer) {}
  SubscriberRegistry(const SubscriberRegistry&) = delete;
  SubscriberRegistry& operator=(const SubscriberRegistry&) = delete;
  ~SubscriberRegistry() = default;

  base::CallbackListSubscription Subscribe(ConsumerCallback<T> callback) {
    ++subscriber_count_;
    base::CallbackListSubscription subscription =
        callback_list_.Add(base::BindRepeating(
            [](ConsumerCallback<T> cb, const base::ScopedClosureRunner&,
               content::Page& page,
               const T& payload) { cb.Run(page, payload); },
            std::move(callback),
            base::OwnedRef(base::ScopedClosureRunner(
                base::BindOnce(&SubscriberRegistry::OnSubscriptionRemoved,
                               weak_ptr_factory_.GetWeakPtr())))));
    producer_->OnDemandChanged();
    return subscription;
  }

  size_t GetSubscriberCount() const { return subscriber_count_; }

  void Notify(content::Page& page, const T& payload) {
    callback_list_.Notify(page, payload);
  }

 private:
  void OnSubscriptionRemoved() {
    CHECK_GT(subscriber_count_, 0u);
    --subscriber_count_;
    producer_->OnDemandChanged();
  }

  size_t subscriber_count_ = 0;
  base::RepeatingCallbackList<void(content::Page&, const T&)> callback_list_;
  const raw_ref<Producer<T>> producer_;
  base::WeakPtrFactory<SubscriberRegistry<T>> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PRODUCER_H_
