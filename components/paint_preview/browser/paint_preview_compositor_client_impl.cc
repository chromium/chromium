// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"

namespace paint_preview {

PaintPreviewCompositorClientImpl::PaintPreviewCompositorClientImpl(
    scoped_refptr<base::SequencedTaskRunner> compositor_task_runner,
    base::WeakPtr<PaintPreviewCompositorServiceImpl> service)
    : compositor_task_runner_(compositor_task_runner),
      default_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      service_(service),
      compositor_(new mojo::Remote<mojom::PaintPreviewCompositor>(),
                  base::OnTaskRunnerDeleter(compositor_task_runner_)) {}

PaintPreviewCompositorClientImpl::~PaintPreviewCompositorClientImpl() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  NotifyServiceOfInvalidation();
}

const std::optional<base::UnguessableToken>&
PaintPreviewCompositorClientImpl::Token() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return token_;
}

void PaintPreviewCompositorClientImpl::SetDisconnectHandler(
    base::OnceClosure closure) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  user_disconnect_closure_ = std::move(closure);
}

// For the following methods the use of base::Unretained for |compositor_| and
// things |compositor_| owns is safe as:
// 1. |compositor_| is deleted on the |compositor_task_runner_| after other
//    non-delayed tasks in the current sequence are run.
// 2. New tasks cannot be created that reference |compositor_| once it is
//    deleted as its lifetime is tied to that of the
//    PaintPreviewCompositorClient.
//
// NOTE: This is only safe as no delayed tasks are posted and there are no
// cases of base::Unretained(this) or other class members passed as pointers.

void PaintPreviewCompositorClientImpl::BeginSeparatedFrameComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    mojom::PaintPreviewCompositor::BeginSeparatedFrameCompositeCallback
        callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::PaintPreviewCompositor::BeginSeparatedFrameComposite,
          base::Unretained(compositor_.get()->get()), std::move(request),
          base::BindPostTask(default_task_runner_, std::move(callback))));
}

void PaintPreviewCompositorClientImpl::BitmapForSeparatedFrame(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    mojom::PaintPreviewCompositor::BitmapForSeparatedFrameCallback callback,
    bool run_callback_on_default_task_runner) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());

  auto task_runner_specified_callback =
      run_callback_on_default_task_runner
          ? base::BindPostTask(default_task_runner_, std::move(callback))
          : std::move(callback);
  auto validate_bitmap = base::BindOnce(
      [](mojom::PaintPreviewCompositor::BitmapForSeparatedFrameCallback
             callback,
         mojom::PaintPreviewCompositor::BitmapStatus status,
         const SkBitmap& bitmap) {
        TRACE_EVENT0("paint_preview",
                     "PaintPreviewCompositorClientImpl::"
                     "BitmapForSeparatedFrameCallback");
        // The paint preview service should be sending us N32 32bpp bitmaps in
        // reply, otherwise this can lead to out-of-bounds mistakes when
        // transferring the pixels out of the bitmap into other buffers.
        CHECK_EQ(bitmap.colorType(), kN32_SkColorType);
        std::move(callback).Run(status, bitmap);
      },
      std::move(task_runner_specified_callback));

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojom::PaintPreviewCompositor::BitmapForSeparatedFrame,
                     base::Unretained(compositor_.get()->get()), frame_guid,
                     clip_rect, scale_factor, std::move(validate_bitmap)));
}

void PaintPreviewCompositorClientImpl::BeginMainFrameComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    mojom::PaintPreviewCompositor::BeginMainFrameCompositeCallback callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::PaintPreviewCompositor::BeginMainFrameComposite,
          base::Unretained(compositor_.get()->get()), std::move(request),
          base::BindPostTask(default_task_runner_, std::move(callback))));
}

void PaintPreviewCompositorClientImpl::BitmapForMainFrame(
    const gfx::Rect& clip_rect,
    float scale_factor,
    mojom::PaintPreviewCompositor::BitmapForMainFrameCallback callback,
    bool run_callback_on_default_task_runner) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());

  auto task_runner_specified_callback =
      run_callback_on_default_task_runner
          ? base::BindPostTask(default_task_runner_, std::move(callback))
          : std::move(callback);
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojom::PaintPreviewCompositor::BitmapForMainFrame,
                     base::Unretained(compositor_.get()->get()), clip_rect,
                     scale_factor, std::move(task_runner_specified_callback)));
}

void PaintPreviewCompositorClientImpl::SetRootFrameUrl(const GURL& url) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojom::PaintPreviewCompositor::SetRootFrameUrl,
                     base::Unretained(compositor_.get()->get()), url));
}

void PaintPreviewCompositorClientImpl::IsBoundAndConnected(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](mojo::Remote<mojom::PaintPreviewCompositor>* compositor,
                        scoped_refptr<base::SequencedTaskRunner> task_runner,
                        base::OnceCallback<void(bool)> callback) {
                       task_runner->PostTask(
                           FROM_HERE,
                           base::BindOnce(std::move(callback),
                                          compositor->is_bound() &&
                                              compositor->is_connected()));
                     },
                     base::Unretained(compositor_.get()), default_task_runner_,
                     std::move(callback)));
}

PaintPreviewCompositorClientImpl::OnCompositorCreatedCallback
PaintPreviewCompositorClientImpl::BuildCompositorCreatedCallback(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return base::BindPostTask(
      default_task_runner_,
      base::BindOnce(&PaintPreviewCompositorClientImpl::OnCompositorCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(user_closure),
                     std::move(service_callback)));
}

void PaintPreviewCompositorClientImpl::OnCompositorCreated(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback,
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  token_ = token;
  std::move(user_closure).Run();
  std::move(service_callback).Run(token);
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojo::Remote<mojom::PaintPreviewCompositor>::set_disconnect_handler,
          base::Unretained(compositor_.get()),
          base::BindPostTask(
              default_task_runner_,
              base::BindOnce(
                  &PaintPreviewCompositorClientImpl::DisconnectHandler,
                  weak_ptr_factory_.GetWeakPtr()))));
}

void PaintPreviewCompositorClientImpl::NotifyServiceOfInvalidation() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  if (service_ && token_.has_value())
    service_->MarkCompositorAsDeleted(token_.value());
}

void PaintPreviewCompositorClientImpl::DisconnectHandler() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  if (user_disconnect_closure_)
    std::move(user_disconnect_closure_).Run();
  NotifyServiceOfInvalidation();
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojo::Remote<mojom::PaintPreviewCompositor>::reset,
                     base::Unretained(compositor_.get())));
}

}  // namespace paint_preview
