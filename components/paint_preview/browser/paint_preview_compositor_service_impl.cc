// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"

namespace paint_preview {

PaintPreviewCompositorServiceImpl::PaintPreviewCompositorServiceImpl(
    mojo::PendingRemote<mojom::PaintPreviewCompositorCollection> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_,
    base::OnceClosure disconnect_handler)
    : default_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      compositor_task_runner_(compositor_task_runner_),
      compositor_service_(
          new mojo::Remote<mojom::PaintPreviewCompositorCollection>(),
          base::OnTaskRunnerDeleter(compositor_task_runner_)),
      user_disconnect_closure_(std::move(disconnect_handler)) {
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::PaintPreviewCompositorCollection>* remote,
             mojo::PendingRemote<mojom::PaintPreviewCompositorCollection>
                 pending,
             base::OnceClosure disconnect_closure) {
            remote->Bind(std::move(pending));
            BindDiscardableSharedMemoryManager(remote);
            remote->set_disconnect_handler(std::move(disconnect_closure));
          },
          compositor_service_.get(), std::move(pending_remote),
          base::BindPostTask(
              default_task_runner_,
              base::BindOnce(
                  &PaintPreviewCompositorServiceImpl::DisconnectHandler,
                  weak_ptr_factory_.GetWeakPtr()))));
}

// The destructor for the |compositor_service_| will automatically result in any
// active compositors being killed.
PaintPreviewCompositorServiceImpl::~PaintPreviewCompositorServiceImpl() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<PaintPreviewCompositorClient, base::OnTaskRunnerDeleter>
PaintPreviewCompositorServiceImpl::CreateCompositor(
    base::OnceClosure connected_closure) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<PaintPreviewCompositorClientImpl, base::OnTaskRunnerDeleter>
      compositor(new PaintPreviewCompositorClientImpl(
                     compositor_task_runner_, weak_ptr_factory_.GetWeakPtr()),
                 base::OnTaskRunnerDeleter(
                     base::SequencedTaskRunner::GetCurrentDefault()));

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::PaintPreviewCompositorCollection>* remote,
             mojo::Remote<mojom::PaintPreviewCompositor>* compositor,
             base::OnceCallback<void(const base::UnguessableToken&)>
                 on_connected) {
            // This binds the remote in compositor to the
            // |compositor_task_runner_|.
            remote->get()->CreateCompositor(
                compositor->BindNewPipeAndPassReceiver(),
                std::move(on_connected));
          },
          // These are both deleted on `compositor_task_runner_` using
          // TaskRunnerDeleter and at this point neither can be scheduled for
          // deletion so passing raw pointers is safe.
          compositor_service_.get(), compositor->GetCompositor(),
          // This builder ensures the callback it returns is called on the
          // correct sequence.
          compositor->BuildCompositorCreatedCallback(
              std::move(connected_closure),
              base::BindOnce(
                  &PaintPreviewCompositorServiceImpl::OnCompositorCreated,
                  weak_ptr_factory_.GetWeakPtr()))));

  return compositor;
}

void PaintPreviewCompositorServiceImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::PaintPreviewCompositorCollection>* remote,
             base::MemoryPressureListener::MemoryPressureLevel
                 memory_pressure_level) {
            if (!remote->is_bound()) {
              return;
            }
            remote->get()->OnMemoryPressure(memory_pressure_level);
          },
          // `compositor_service_` is only deleted on `compositor_task_runner_`
          // using TaskRunnerDeleter. Since the parent object is alive at this
          // point passing a pointer is safe.
          compositor_service_.get(), memory_pressure_level));
}

bool PaintPreviewCompositorServiceImpl::HasActiveClients() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return !active_clients_.empty();
}

void PaintPreviewCompositorServiceImpl::SetDisconnectHandler(
    base::OnceClosure disconnect_handler) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  user_disconnect_closure_ = std::move(disconnect_handler);
}

void PaintPreviewCompositorServiceImpl::MarkCompositorAsDeleted(
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  active_clients_.erase(token);
}

const base::flat_set<base::UnguessableToken>&
PaintPreviewCompositorServiceImpl::ActiveClientsForTesting() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return active_clients_;
}

void PaintPreviewCompositorServiceImpl::OnCompositorCreated(
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  active_clients_.insert(token);
}

void PaintPreviewCompositorServiceImpl::DisconnectHandler() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  if (user_disconnect_closure_)
    std::move(user_disconnect_closure_).Run();

  // Don't call `compositor_service_.reset()` so the remote stays bound and ptrs
  // to it in callbacks remain valid. Disconnect calls will just drop silently.
}

}  // namespace paint_preview
