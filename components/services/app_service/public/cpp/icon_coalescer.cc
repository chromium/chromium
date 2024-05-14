// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_coalescer.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace apps {

// scoped_refptr<RefCountedReleaser> converts a
// std::unique_ptr<IconLoader::Releaser> to a ref-counted pointer.
class IconCoalescer::RefCountedReleaser
    : public base::RefCounted<RefCountedReleaser> {
 public:
  explicit RefCountedReleaser(std::unique_ptr<IconLoader::Releaser> releaser);

  RefCountedReleaser(const RefCountedReleaser&) = delete;
  RefCountedReleaser& operator=(const RefCountedReleaser&) = delete;

 private:
  friend class base::RefCounted<RefCountedReleaser>;

  virtual ~RefCountedReleaser();

  std::unique_ptr<IconLoader::Releaser> releaser_;
};

IconCoalescer::RefCountedReleaser::RefCountedReleaser(
    std::unique_ptr<IconLoader::Releaser> releaser)
    : releaser_(std::move(releaser)) {}

IconCoalescer::RefCountedReleaser::~RefCountedReleaser() = default;

IconCoalescer::IconCoalescer(IconLoader* wrapped_loader)
    : wrapped_loader_(wrapped_loader), next_sequence_number_(0) {}

IconCoalescer::~IconCoalescer() = default;

std::optional<IconKey> IconCoalescer::GetIconKey(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wrapped_loader_ ? wrapped_loader_->GetIconKey(id) : std::nullopt;
}

std::unique_ptr<IconLoader::Releaser> IconCoalescer::LoadIconFromIconKey(
    const std::string& id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!wrapped_loader_) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  if (icon_type != IconType::kUncompressed &&
      icon_type != IconType::kStandard) {
    return wrapped_loader_->LoadIconFromIconKey(
        id, icon_key, icon_type, size_hint_in_dip, allow_placeholder_icon,
        std::move(callback));
  }

  scoped_refptr<RefCountedReleaser> shared_releaser;
  IconLoader::Key key(id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon);

  auto iter = non_immediate_requests_.find(key);
  if (iter != non_immediate_requests_.end()) {
    // Coalesce this request with an in-flight one.
    //
    // |iter->second| is a CallbackAndReleaser. |iter->second.second| is a
    // scoped_refptr<RefCountedReleaser>.
    shared_releaser = iter->second.second;
  } else {
    // There is no in-flight request to coalesce with. Instead, forward on the
    // request to the wrapped IconLoader.
    //
    // Calling the |wrapped_loader_|'s LoadIconFromIconKey implementation might
    // invoke the passed OnceCallback (binding this class' OnLoadIcon method)
    // immediately (now), or at a later time. In both cases, we have to invoke
    // (now or later) the |callback| that was passed to this function.
    //
    // If it's later, then we stash |callback| in |non_immediate_requests_|,
    // and look up that same |non_immediate_requests_| during OnLoadIcon.
    //
    // If it's now, then inserting into the |non_immediate_requests_| would be
    // tricky, as we'd have to then unstash the |callback| out of the
    // |non_immediate_requests_| (recall that a OnceCallback can be std::move'd
    // but not copied), but there are potentially multiple entries with the
    // same key, and any multimap iterator might be invalidated if calling into
    // the |wrapped_loader_| caused other code to call back into this
    // IconCoalescer and mutate that multimap.
    //
    // Instead, |possibly_immediate_requests_| and |immediate_responses_| keeps
    // track of now vs later.
    //
    // If it's now (if OnLoadIcon is called when the current |seq_num| is in
    // |possibly_immediate_requests_|), then OnLoadIcon will populate
    // |immediate_responses_| with that |seq_num|. We then run |callback| now,
    // right after |wrapped_loader_->LoadIconFromIconKey| returns.
    //
    // Otherwise we have asynchronously dispatched the underlying icon loading
    // request, so store |callback| in |non_immediate_requests_| to be run
    // later, when the asynchronous request resolves.
    uint64_t seq_num = next_sequence_number_++;
    possibly_immediate_requests_.insert(seq_num);

    std::unique_ptr<IconLoader::Releaser> unique_releaser =
        wrapped_loader_->LoadIconFromIconKey(
            id, icon_key, icon_type, size_hint_in_dip, allow_placeholder_icon,
            base::BindOnce(&IconCoalescer::OnLoadIcon,
                           weak_ptr_factory_.GetWeakPtr(), key, seq_num));

    possibly_immediate_requests_.erase(seq_num);

    auto iv_iter = immediate_responses_.find(seq_num);
    if (iv_iter != immediate_responses_.end()) {
      IconValuePtr iv = std::move(iv_iter->second);
      immediate_responses_.erase(iv_iter);
      std::move(callback).Run(std::move(iv));
      return unique_releaser;
    }

    shared_releaser =
        base::MakeRefCounted<RefCountedReleaser>(std::move(unique_releaser));
  }

  non_immediate_requests_.insert(std::make_pair(
      key, std::make_pair(std::move(callback), shared_releaser)));

  return std::make_unique<IconLoader::Releaser>(
      nullptr,
      // The callback does nothing explicitly, but after it runs, it implicitly
      // decrements the scoped_refptr's shared reference count, and therefore
      // possibly deletes the underlying IconLoader::Releaser.
      base::DoNothingWithBoundArgs(std::move(shared_releaser)));
}

void IconCoalescer::OnLoadIcon(IconLoader::Key key,
                               uint64_t sequence_number,
                               IconValuePtr icon_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (possibly_immediate_requests_.find(sequence_number) !=
      possibly_immediate_requests_.end()) {
    immediate_responses_.insert(
        std::make_pair(sequence_number, std::move(icon_value)));
    return;
  }

  auto range = non_immediate_requests_.equal_range(key);
  auto count = std::distance(range.first, range.second);
  if (count <= 0) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Optimize / simplify the common case.
  if (count == 1) {
    CallbackAndReleaser callback_and_releaser = std::move(range.first->second);
    non_immediate_requests_.erase(range.first, range.second);
    std::move(callback_and_releaser.first).Run(std::move(icon_value));
    return;
  }

  // Run every callback in |range|. This is subtle, because an arbitrary
  // callback could invoke further methods on |this|, which could mutate
  // |non_immediate_requests_|, invalidating |range|'s iterators.
  //
  // Thus, we first gather the callbacks, then erase the |range|, then run the
  // callbacks.
  //
  // We still run the callbacks, synchronously, instead of posting them on a
  // task runner to run later, asynchronously, even though using a task runner
  // could avoid having to separate gathering and running the callbacks.
  // Synchronous invocation keep the call stack's "how did I get here"
  // information, which is useful when debugging.

  std::vector<apps::LoadIconCallback> callbacks;
  callbacks.reserve(count);
  for (auto iter = range.first; iter != range.second; ++iter) {
    // |iter->second| is a CallbackAndReleaser. |iter->second.first| is a
    // LoadIconCallback.
    callbacks.push_back(std::move(iter->second.first));
  }

  non_immediate_requests_.erase(range.first, range.second);

  for (auto& callback : callbacks) {
    IconValuePtr iv;
    if (--count == 0) {
      iv = std::move(icon_value);
    } else {
      iv = std::make_unique<IconValue>();
      iv->icon_type = icon_value->icon_type;
      iv->uncompressed = icon_value->uncompressed;
      iv->is_placeholder_icon = icon_value->is_placeholder_icon;
    }
    std::move(callback).Run(std::move(iv));
  }
}

}  // namespace apps
