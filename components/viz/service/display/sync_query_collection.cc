// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/sync_query_collection.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/container_util.h"
#include "components/viz/service/display/resource_fence.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace viz {
namespace {
// Block or crash if the number of pending sync queries reach this high as
// something is seriously wrong on the service side if this happens.
const size_t kMaxPendingSyncQueries = 16;
}  // anonymous namespace

class SyncQuery {
 public:
  explicit SyncQuery(gpu::gles2::GLES2Interface* gl)
      : gl_(gl), query_id_(0u), is_pending_(false) {
    gl_->GenQueriesEXT(1, &query_id_);
  }
  virtual ~SyncQuery() { gl_->DeleteQueriesEXT(1, &query_id_); }

  scoped_refptr<ResourceFence> Begin() {
    DCHECK(!IsPending());
    // Invalidate weak pointer held by old fence.
    weak_ptr_factory_.InvalidateWeakPtrs();
    // Note: In case the set of drawing commands issued before End() do not
    // depend on the query, defer BeginQueryEXT call until Set() is called and
    // query is required.
    return base::MakeRefCounted<Fence>(weak_ptr_factory_.GetWeakPtr());
  }

  void Set() {
    if (is_pending_)
      return;

    // Note: BeginQueryEXT on GL_COMMANDS_COMPLETED_CHROMIUM is effectively a
    // noop relative to GL, so it doesn't matter where it happens but we still
    // make sure to issue this command when Set() is called (prior to issuing
    // any drawing commands that depend on query), in case some future extension
    // can take advantage of this.
    gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id_);
    is_pending_ = true;
  }

  void End() {
    if (!is_pending_)
      return;

    gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  }

  bool IsPending() {
    if (!is_pending_)
      return false;

    unsigned result_available = 1;
    gl_->GetQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_AVAILABLE_EXT,
                              &result_available);
    is_pending_ = !result_available;
    return is_pending_;
  }

  void Wait() {
    if (!is_pending_)
      return;

    unsigned result = 0;
    gl_->GetQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_EXT, &result);
    is_pending_ = false;
  }

 private:
  class Fence : public ResourceFence {
   public:
    explicit Fence(base::WeakPtr<SyncQuery> query) : query_(query) {}

    // ResourceFence implementation.
    void Set() override {
      DCHECK(query_);
      query_->Set();
    }
    bool HasPassed() override { return !query_ || !query_->IsPending(); }

   private:
    ~Fence() override {}

    base::WeakPtr<SyncQuery> query_;

    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  gpu::gles2::GLES2Interface* gl_;
  unsigned query_id_;
  bool is_pending_;
  base::WeakPtrFactory<SyncQuery> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncQuery);
};

SyncQueryCollection::SyncQueryCollection(gpu::gles2::GLES2Interface* gl)
    : gl_(gl) {}

SyncQueryCollection::~SyncQueryCollection() = default;
SyncQueryCollection::SyncQueryCollection(SyncQueryCollection&&) = default;
SyncQueryCollection& SyncQueryCollection::operator=(SyncQueryCollection&&) =
    default;

scoped_refptr<ResourceFence> SyncQueryCollection::StartNewFrame() {
  // Block until oldest sync query has passed if the number of pending queries
  // ever reach kMaxPendingSyncQueries.
  if (pending_sync_queries_.size() >= kMaxPendingSyncQueries) {
    LOG(ERROR) << "Reached limit of pending sync queries.";

    pending_sync_queries_.front()->Wait();
    DCHECK(!pending_sync_queries_.front()->IsPending());
  }

  while (!pending_sync_queries_.empty()) {
    if (pending_sync_queries_.front()->IsPending())
      break;

    available_sync_queries_.push_back(cc::PopFront(&pending_sync_queries_));
  }

  current_sync_query_ = available_sync_queries_.empty()
                            ? std::make_unique<SyncQuery>(gl_)
                            : cc::PopFront(&available_sync_queries_);

  return current_sync_query_->Begin();
}

void SyncQueryCollection::EndCurrentFrame() {
  DCHECK(current_sync_query_);
  current_sync_query_->End();
  pending_sync_queries_.push_back(std::move(current_sync_query_));
}

}  // namespace viz
