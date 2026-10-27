// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility.h"

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/optimization_guide/core/optimization_guide_library_holder.h"

namespace optimization_guide {

PageContextEligibility::PageContextEligibility(
    const PageContextEligibilityAPI* api)
    : api_(api) {}
PageContextEligibility::~PageContextEligibility() = default;

namespace {

PageContextEligibility* g_page_context_eligibility_for_testing = nullptr;

// Coordinates concurrent `GetAsync()` requests across calling sequences with
// the background `ThreadPool` worker task that loads the singleton instance.
// All state transitions and the pending callback queue are guarded by `lock`.
struct AsyncLoadState {
  base::Lock lock;
  bool is_loading GUARDED_BY(lock) = false;
  bool is_loaded GUARDED_BY(lock) = false;
  raw_ptr<PageContextEligibility> instance GUARDED_BY(lock) = nullptr;
  std::vector<base::OnceCallback<void(PageContextEligibility*)>>
      pending_callbacks GUARDED_BY(lock);
};

AsyncLoadState& GetAsyncLoadState() {
  static base::NoDestructor<AsyncLoadState> state;
  return *state;
}

}  // namespace

// static
DISABLE_CFI_DLSYM
PageContextEligibility* PageContextEligibility::Get() {
  if (g_page_context_eligibility_for_testing) {
    return g_page_context_eligibility_for_testing;
  }
  return GetLoadedInstance();
}

// static
DISABLE_CFI_DLSYM
PageContextEligibility* PageContextEligibility::GetLoadedInstance() {
  static base::NoDestructor<std::unique_ptr<PageContextEligibility>>
      page_context_eligibility{Create()};
  return page_context_eligibility->get();
}

// static
void PageContextEligibility::GetAsync(
    base::OnceCallback<void(PageContextEligibility*)> callback) {
  if (g_page_context_eligibility_for_testing) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceCallback<void(PageContextEligibility*)> cb) {
              std::move(cb).Run(g_page_context_eligibility_for_testing);
            },
            std::move(callback)));
    return;
  }

  AsyncLoadState& state = GetAsyncLoadState();
  bool should_post_worker_task = false;
  {
    base::AutoLock auto_lock(state.lock);
    if (state.is_loaded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), state.instance));
      return;
    }
    state.pending_callbacks.push_back(
        base::BindPostTaskToCurrentDefault(std::move(callback)));
    if (!state.is_loading) {
      state.is_loading = true;
      should_post_worker_task = true;
    }
  }

  if (should_post_worker_task) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce([]() {
          PageContextEligibility* instance =
              PageContextEligibility::GetLoadedInstance();
          std::vector<base::OnceCallback<void(PageContextEligibility*)>>
              callbacks;
          {
            AsyncLoadState& state = GetAsyncLoadState();
            base::AutoLock auto_lock(state.lock);
            state.instance = instance;
            state.is_loaded = true;
            state.is_loading = false;
            callbacks = std::exchange(state.pending_callbacks, {});
          }
          for (auto& cb : callbacks) {
            std::move(cb).Run(instance);
          }
        }));
  }
}

// static
void PageContextEligibility::SetForTesting(PageContextEligibility* api_holder) {
  g_page_context_eligibility_for_testing = api_holder;
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<PageContextEligibility> PageContextEligibility::Create() {
  OptimizationGuideLibraryHolder* holder_ptr =
      OptimizationGuideLibraryHolder::GetInstance();
  if (!holder_ptr) {
    return {};
  }

  auto get_api = reinterpret_cast<PageContextEligibilityAPIGetter>(
      holder_ptr->GetFunctionPointer("GetPageContextEligibilityAPI"));
  if (!get_api) {
    return {};
  }

  const PageContextEligibilityAPI* api = get_api();
  if (!api) {
    return {};
  }
  return base::WrapUnique(new PageContextEligibility(api));
}

std::vector<FrameMetadata> GetFrameMetadataFromPageContent(
    const AIPageContentResult& result) {
  std::vector<FrameMetadata> frame_metadata_structs;
  const auto& page_metadata = result.metadata;
  frame_metadata_structs.reserve(page_metadata->frame_metadata.size());
  for (auto& frame_metadata_mojom : page_metadata->frame_metadata) {
    std::vector<MetaTag> meta_tags;
    meta_tags.reserve(frame_metadata_mojom->meta_tags.size());
    for (auto& tag : frame_metadata_mojom->meta_tags) {
      MetaTag meta_tag(tag->name, tag->content);
      meta_tags.push_back(std::move(meta_tag));
    }
    FrameMetadata metadata(frame_metadata_mojom->url.GetHost(),
                           frame_metadata_mojom->url.GetPath(),
                           std::move(meta_tags));
    frame_metadata_structs.push_back(std::move(metadata));
  }
  return frame_metadata_structs;
}

DISABLE_CFI_DLSYM
bool IsPageContextEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata,
    const PageContextEligibility* api_holder) {
  // TODO(crbug.com/421932889): `api_holder` should not be provided by caller
  // and instead be retrieved as part of this function call.
  if (!api_holder) {
    return true;
  }

  return api_holder->api().IsPageContextEligible(host, path,
                                                 std::move(frame_metadata));
}

DISABLE_CFI_DLSYM
bool IsPageContextEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata,
    const PageContextEligibility* api_holder) {
  if (!api_holder) {
    return true;
  }

  return api_holder->api().IsPageContextEligibleWithAccount(host, path, account,
                                                            frame_metadata);
}

DISABLE_CFI_DLSYM
PageEligibilityResult CheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>& frames,
    const PageContextEligibility* api_holder) {
  if (!api_holder) {
    return PageEligibilityResult{
        .status = PageEligibility::kEligible,
        .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
  }

  return api_holder->api().CheckPageEligibility(frames);
}

}  // namespace optimization_guide
