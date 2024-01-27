// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

using CompositorPtr =
    std::unique_ptr<mojo::Remote<mojom::PaintPreviewCompositor>,
                    base::OnTaskRunnerDeleter>;

// The implementation of the PaintPreviewCompositorClient class.
// The public interface should be invoked only on the |default_task_runner_|
// which is the the runner returned by
// base::SequencedTaskRunner::GetCurrentDefault() when this is constructed.
class PaintPreviewCompositorClientImpl : public PaintPreviewCompositorClient {
 public:
  using OnCompositorCreatedCallback =
      base::OnceCallback<void(const base::UnguessableToken&)>;

  explicit PaintPreviewCompositorClientImpl(
      scoped_refptr<base::SequencedTaskRunner> compositor_task_runner,
      base::WeakPtr<PaintPreviewCompositorServiceImpl> service);
  ~PaintPreviewCompositorClientImpl() override;

  // PaintPreviewCompositorClient implementation.
  const std::optional<base::UnguessableToken>& Token() const override;
  void SetDisconnectHandler(base::OnceClosure closure) override;
  void BeginSeparatedFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginSeparatedFrameCompositeCallback
          callback) override;
  void BitmapForSeparatedFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForSeparatedFrameCallback callback,
      bool run_callback_on_default_task_runner = true) override;
  void BeginMainFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginMainFrameCompositeCallback callback)
      override;
  void BitmapForMainFrame(
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForMainFrameCallback callback,
      bool run_callback_on_default_task_runner = true) override;
  void SetRootFrameUrl(const GURL& url) override;

  // The returned remote should only be used on `compositor_task_runner_`.
  mojo::Remote<mojom::PaintPreviewCompositor>* GetCompositor() {
    return compositor_.get();
  }

  void IsBoundAndConnected(base::OnceCallback<void(bool)> callback);

  OnCompositorCreatedCallback BuildCompositorCreatedCallback(
      base::OnceClosure user_closure,
      OnCompositorCreatedCallback service_callback);

  PaintPreviewCompositorClientImpl(const PaintPreviewCompositorClientImpl&) =
      delete;
  PaintPreviewCompositorClientImpl& operator=(
      const PaintPreviewCompositorClientImpl&) = delete;

 private:
  void OnCompositorCreated(base::OnceClosure user_closure,
                           OnCompositorCreatedCallback service_callback,
                           const base::UnguessableToken& token);

  void NotifyServiceOfInvalidation();

  void DisconnectHandler();

  scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  base::WeakPtr<PaintPreviewCompositorServiceImpl> service_;
  CompositorPtr compositor_;

  std::optional<base::UnguessableToken> token_;
  base::OnceClosure user_disconnect_closure_;

  base::WeakPtrFactory<PaintPreviewCompositorClientImpl> weak_ptr_factory_{
      this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_
