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

// OVERVIEW:
//
// Demand-driven subscription primitives for stages in the page content
// pipeline. A `Producer<T>` asynchronously computes per-page artifacts of type
// `T` and composes a `SubscriberRegistry<T>` to manage active subscriptions,
// broadcast newly produced artifacts, and react when downstream demand starts
// or stops.
//
// TYPICAL USAGE:
//
// class MyStageService : public Producer<MyArtifact> {
//  public:
//   explicit MyStageService(Producer<UpstreamArtifact>& upstream)
//       : upstream_(upstream) {}
//
//   base::CallbackListSubscription Subscribe(
//       ConsumerCallback<MyArtifact> callback) override {
//     return subscriber_registry_.Subscribe(std::move(callback));
//   }
//
//  private:
//   void OnSubscribersChanged() override {
//     if (!subscriber_registry_.HasSubscribers()) {
//       upstream_subscription_.reset();
//       return;
//     }
//
//     if (!upstream_subscription_) {
//       upstream_subscription_ = upstream_->Subscribe(base::BindRepeating(
//           &MyStageService::OnUpstreamArtifact, base::Unretained(this)));
//     }
//   }
//
//   void OnUpstreamArtifact(content::Page& page,
//                           const UpstreamArtifact& upstream_artifact) {
//     ComputeArtifactAsync(
//         upstream_artifact,
//         base::BindOnce(&MyStageService::OnArtifactComputed,
//                        weak_ptr_factory_.GetWeakPtr(), page.GetWeakPtr()));
//   }
//
//   void OnArtifactComputed(base::WeakPtr<content::Page> page,
//                           MyArtifact artifact) {
//     if (!page) {
//       return;
//     }
//     subscriber_registry_.Notify(*page, artifact);
//   }
//
//   const raw_ref<Producer<UpstreamArtifact>> upstream_;
//   std::optional<base::CallbackListSubscription> upstream_subscription_;
//   SubscriberRegistry<MyArtifact> subscriber_registry_{*this};
//   base::WeakPtrFactory<MyStageService> weak_ptr_factory_{this};
// };

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

  // Registers `callback` to receive artifacts produced while the returned
  // subscription remains alive. Only delivers artifacts produced after
  // subscribing; artifacts produced prior to subscribing are not replayed.
  virtual base::CallbackListSubscription Subscribe(
      ConsumerCallback<T> callback) = 0;

 protected:
  friend class SubscriberRegistry<T>;

  // Invoked synchronously by `SubscriberRegistry<T>` whenever a subscriber is
  // added or removed so the producer can re-evaluate downstream demand and
  // start or stop upstream subscriptions or background work accordingly.
  virtual void OnSubscribersChanged() = 0;
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
    // Run `OnSubscriptionRemoved()` when the subscription is destroyed.
    base::ScopedClosureRunner on_removed_runner(
        base::BindOnce(&SubscriberRegistry::OnSubscriptionRemoved,
                       weak_ptr_factory_.GetWeakPtr()));
    callback = std::move(callback).Then(
        base::RepeatingClosure(base::DoNothingWithBoundArgs(
            base::OwnedRef(std::move(on_removed_runner)))));
    base::CallbackListSubscription subscription =
        callback_list_.Add(std::move(callback));
    producer_->OnSubscribersChanged();
    return subscription;
  }

  bool HasSubscribers() const { return subscriber_count_ > 0; }
  size_t GetSubscriberCount() const { return subscriber_count_; }

  void Notify(content::Page& page, const T& payload) {
    callback_list_.Notify(page, payload);
  }

 private:
  void OnSubscriptionRemoved() {
    CHECK_GT(subscriber_count_, 0u);
    --subscriber_count_;
    producer_->OnSubscribersChanged();
  }

  size_t subscriber_count_ = 0;
  base::RepeatingCallbackList<void(content::Page&, const T&)> callback_list_;
  const raw_ref<Producer<T>> producer_;
  base::WeakPtrFactory<SubscriberRegistry<T>> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PRODUCER_H_
