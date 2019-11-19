// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_BROWSER_PROCESS_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_BROWSER_PROCESS_H_

#include <vector>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"
#include "components/viz/service/compositor_frame_fuzzer/fuzzer_software_output_surface_provider.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/fake_display_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace viz {

// A fake browser process to use as a fuzzer target. Uses software compositing.
class FuzzerBrowserProcess {
 public:
  explicit FuzzerBrowserProcess(base::Optional<base::FilePath> png_dir_path);
  ~FuzzerBrowserProcess();

  // Fuzz target mimicking the process of submitting a rendered CompositorFrame
  // to be embedded in the browser UI.
  //
  // Submits the provided fuzzed CompositorFrame to a new
  // CompositorFrameSinkImpl.
  //
  // Submits a CompositorFrame to the RootCompositorFrameSinkImpl
  // with a SolidColorDrawQuad "toolbar" and a SurfaceDrawQuad "renderer frame"
  // embedding the fuzzed CompositorFrame.
  //
  // |allocated_bitmaps| should contain references to already-allocated memory
  // that is referenced by the frame's DrawQuads and |resource_list|.
  void EmbedFuzzedCompositorFrame(CompositorFrame fuzzed_frame,
                                  std::vector<FuzzedBitmap> allocated_bitmaps);

 private:
  mojom::RootCompositorFrameSinkParamsPtr BuildRootCompositorFrameSinkParams();
  CompositorFrame BuildBrowserUICompositorFrame(SurfaceId renderer_surface_id);

  const LocalSurfaceId root_local_surface_id_;

  ServerSharedBitmapManager shared_bitmap_manager_;
  FuzzerSoftwareOutputSurfaceProvider output_surface_provider_;
  FrameSinkManagerImpl frame_sink_manager_;

  mojo::AssociatedRemote<mojom::CompositorFrameSink>
      root_compositor_frame_sink_remote_;
  FakeCompositorFrameSinkClient root_compositor_frame_sink_client_;
  mojo::AssociatedRemote<mojom::DisplayPrivate> display_private_;
  FakeDisplayClient display_client_;
  mojo::AssociatedRemote<mojom::ExternalBeginFrameController>
      external_begin_frame_controller_remote_;

  ParentLocalSurfaceIdAllocator lsi_allocator_;

  FrameTokenGenerator next_frame_token_;

  DISALLOW_COPY_AND_ASSIGN(FuzzerBrowserProcess);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_BROWSER_PROCESS_H_
