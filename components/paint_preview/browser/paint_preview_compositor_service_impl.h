// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

using CompositorCollectionPtr =
    std::unique_ptr<mojo::Remote<mojom::PaintPreviewCompositorCollection>,
                    base::OnTaskRunnerDeleter>;

// The implementation of the PaintPreviewCompositorService class.
// The public interface should be invoked only on the |default_task_runner_|
// which is the the runner returned by
// base::SequencedTaskRunner::GetCurrentDefault() when this is constructed.
class PaintPreviewCompositorServiceImpl : public PaintPreviewCompositorService {
 public:
  explicit PaintPreviewCompositorServiceImpl(
      mojo::PendingRemote<mojom::PaintPreviewCompositorCollection>
          pending_remote,
      scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_,
      base::OnceClosure disconnect_closure);
  ~PaintPreviewCompositorServiceImpl() override;

  // PaintPreviewCompositorService Implementation.
  std::unique_ptr<PaintPreviewCompositorClient, base::OnTaskRunnerDeleter>
  CreateCompositor(base::OnceClosure connected_closure) override;
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel
                            memory_pressure_level) override;

  bool HasActiveClients() const override;
  // NOTE: this is set by the constructor. However, in some cases it may need to
  // be changed.
  void SetDisconnectHandler(base::OnceClosure disconnect_handler) override;

  // Marks the compositor associated with |token| as deleted in the
  // |active_clients_| set.
  void MarkCompositorAsDeleted(const base::UnguessableToken& token);

  // Test method to validate internal state.
  const base::flat_set<base::UnguessableToken>& ActiveClientsForTesting() const;

  PaintPreviewCompositorServiceImpl(const PaintPreviewCompositorServiceImpl&) =
      delete;
  PaintPreviewCompositorServiceImpl& operator=(
      const PaintPreviewCompositorServiceImpl&) = delete;

 private:
  void OnCompositorCreated(const base::UnguessableToken& token);

  void DisconnectHandler();

  base::flat_set<base::UnguessableToken> active_clients_;

  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_;

  // Bound to |compositor_task_runner_| all methods for the remote owned by this
  // must be called on |compositor_task_runner_| this is deleted on the same
  // task runner so that using it with base::Unretained is safe.
  CompositorCollectionPtr compositor_service_;

  // Called on |default_task_runner_|.
  base::OnceClosure user_disconnect_closure_;

  base::WeakPtrFactory<PaintPreviewCompositorServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_
