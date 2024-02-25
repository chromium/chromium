// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {

namespace mojom {
class CopyOutputRequestDataView;
}

// Holds all the properties pertaining to a copy of a surface or layer.
// Implementations that execute these requests must provide the requested
// ResultFormat or else an "empty" result. Likewise, this means that any
// transient or permanent errors preventing the successful execution of a
// copy request will result in an "empty" result.
//
// Usage: Client code creates a CopyOutputRequest, optionally sets some/all of
// its properties, and then submits it to the compositing pipeline via one of a
// number of possible entry points (usually methods named RequestCopyOfOutput()
// or RequestCopyOfSurface()). Then, some time later, the given result callback
// will be run and the client processes the CopyOutputResult containing the
// image.
//
// Note: This should be used for one-off screen capture only, and NOT for video
// screen capture use cases (please use FrameSinkVideoCapturer instead).
class VIZ_COMMON_EXPORT CopyOutputRequest {
 public:
  using ResultFormat = CopyOutputResult::Format;
  // Specifies intended destination for the results. For software compositing,
  // only the system-memory results are supported - even if the
  // CopyOutputRequest is issued with ResultDestination::kNativeTextures, the
  // results will still be returned via ResultDestination::kSystemMemory.
  using ResultDestination = CopyOutputResult::Destination;

  using CopyOutputRequestCallback =
      base::OnceCallback<void(std::unique_ptr<CopyOutputResult> result)>;

  // Creates new CopyOutputRequest. I420_PLANES format returned via
  // kNativeTextures is currently not supported.
  CopyOutputRequest(ResultFormat result_format,
                    ResultDestination result_destination,
                    CopyOutputRequestCallback result_callback);

  CopyOutputRequest(const CopyOutputRequest&) = delete;
  CopyOutputRequest& operator=(const CopyOutputRequest&) = delete;

  virtual ~CopyOutputRequest();

  // Returns the requested result format.
  ResultFormat result_format() const { return result_format_; }
  // Returns the requested result destination.
  ResultDestination result_destination() const { return result_destination_; }

  // Requests that the result callback be run as a task posted to the given
  // |task_runner|. If this is not set, the result callback could be run from
  // any context.
  void set_result_task_runner(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    result_task_runner_ = std::move(task_runner);
  }
  bool has_result_task_runner() const { return !!result_task_runner_; }

  // Optionally specify that the result should be scaled. |scale_from| and
  // |scale_to| describe the scale ratio in terms of relative sizes: Downscale
  // if |scale_from| > |scale_to|, upscale if |scale_from| < |scale_to|, and
  // no scaling if |scale_from| == |scale_to|. Neither argument may be zero.
  //
  // There are two setters: SetScaleRatio() allows for requesting an arbitrary
  // scale in each dimension, which is sometimes useful for minor "tweaks" that
  // optimize visual quality. SetUniformScaleRatio() scales both dimensions by
  // the same amount.
  void SetScaleRatio(const gfx::Vector2d& scale_from,
                     const gfx::Vector2d& scale_to);
  void SetUniformScaleRatio(int scale_from, int scale_to);
  const gfx::Vector2d& scale_from() const { return scale_from_; }
  const gfx::Vector2d& scale_to() const { return scale_to_; }
  bool is_scaled() const { return scale_from_ != scale_to_; }

  // Optionally specify the source of this copy request. This is used when the
  // client plans to make many similar copy requests over short periods of time.
  // It is used to: 1) auto-abort prior uncommitted copy requests to avoid
  // duplicate copies of the same frame; and 2) hint to the implementation to
  // cache resources for more-efficient execution of later copy requests.
  void set_source(const base::UnguessableToken& source) { source_ = source; }
  bool has_source() const { return source_.has_value(); }
  const base::UnguessableToken& source() const { return *source_; }

  // Optionally specify the clip rect; meaning that just a portion of the entire
  // surface (or layer's subtree output) should be scanned to produce a result.
  // This rect is in the same space as the RenderPass output rect, pre-scaling.
  // This is related to set_result_selection() (see below).
  void set_area(const gfx::Rect& area) { area_ = area; }
  bool has_area() const { return area_.has_value(); }
  const gfx::Rect& area() const { return *area_; }

  // Optionally specify that only a portion of the result be generated. The
  // selection rect will be clamped to the result bounds, which always starts at
  // 0,0 and spans the post-scaling size of the copy area (see set_area()
  // above). Only RGBA format supports odd-sized result selection. Can only be
  // called before blit request was set on the copy request.
  void set_result_selection(const gfx::Rect& selection) {
    DCHECK(result_format_ == ResultFormat::RGBA ||
           (selection.width() % 2 == 0 && selection.height() % 2 == 0))
        << "CopyOutputRequest supports odd-sized result_selection() only for "
           "RGBA!";
    DCHECK(!has_blit_request());
    result_selection_ = selection;
  }
  bool has_result_selection() const { return result_selection_.has_value(); }
  const gfx::Rect& result_selection() const { return *result_selection_; }

  // Requests that the region copied by the CopyOutputRequest be blitted into
  // the caller's textures. Can be called only for CopyOutputRequests that
  // target native textures. Requires that result selection was set, in which
  // case the caller's textures will be populated with the results of the
  // copy request. The region in the caller's textures that will be populated
  // is specified by `gfx::Rect(blit_request.destination_region_offset(),
  // result_selection().size())`. If blit request is configured to perform
  // letterboxing, all contents outside of that region will be overwritten with
  // black, otherwise they will be unchanged. If the copy request's result would
  // be smaller than `result_selection().size()`, the request will fail (i.e.
  // empty result will be sent).
  void set_blit_request(BlitRequest blit_request);
  bool has_blit_request() const { return blit_request_.has_value(); }
  const BlitRequest& blit_request() const { return *blit_request_; }

  // Sends the result from executing this request. Called by the internal
  // implementation, usually a DirectRenderer.
  void SendResult(std::unique_ptr<CopyOutputResult> result);

  // Returns true if SendResult() will deliver the CopyOutputResult using the
  // same TaskRunner as that to which the current task was posted.
  bool SendsResultsInCurrentSequence() const;

  // Creates a RGBA request with ResultDestination::kSystemMemory that ignores
  // results, for testing purposes.
  static std::unique_ptr<CopyOutputRequest> CreateStubForTesting();

  std::string ToString() const;

 private:
  // Note: The StructTraits may "steal" the |result_callback_|, to allow it to
  // outlive this CopyOutputRequest (and wait for the result from another
  // process).
  friend struct mojo::StructTraits<mojom::CopyOutputRequestDataView,
                                   std::unique_ptr<CopyOutputRequest>>;

  const ResultFormat result_format_;
  const ResultDestination result_destination_;
  CopyOutputRequestCallback result_callback_;
  scoped_refptr<base::SequencedTaskRunner> result_task_runner_;
  gfx::Vector2d scale_from_;
  gfx::Vector2d scale_to_;
  std::optional<base::UnguessableToken> source_;
  std::optional<gfx::Rect> area_;
  std::optional<gfx::Rect> result_selection_;

  std::optional<BlitRequest> blit_request_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
