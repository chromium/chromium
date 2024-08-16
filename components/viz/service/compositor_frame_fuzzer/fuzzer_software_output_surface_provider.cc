// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/fuzzer_software_output_surface_provider.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/codec/png_codec.h"

namespace viz {

namespace {

// SoftwareOutputDevice that dumps the display's pixmap into a new PNG file per
// paint event. For debugging only: this significantly slows down fuzzing
// iterations and will not handle more than UINT32_MAX files.
class PNGSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  explicit PNGSoftwareOutputDevice(base::FilePath output_dir)
      : output_dir_(output_dir) {}

  // SoftwareOutputDevice implementation
  void EndPaint() override {
    SkPixmap input_pixmap;
    surface_->peekPixels(&input_pixmap);

    gfx::PNGCodec::ColorFormat color_format;
    switch (input_pixmap.colorType()) {
      case kRGBA_8888_SkColorType:
        color_format = gfx::PNGCodec::FORMAT_RGBA;
        break;
      case kBGRA_8888_SkColorType:
        color_format = gfx::PNGCodec::FORMAT_BGRA;
        break;
      default:
        // failing to find a better default, this one is OK; PNGCodec::Encode
        // will convert this to kN32_SkColorType
        color_format = gfx::PNGCodec::FORMAT_SkBitmap;
        break;
    }

    std::vector<unsigned char> output;
    gfx::PNGCodec::Encode(
        static_cast<const unsigned char*>(input_pixmap.addr()), color_format,
        gfx::Size(input_pixmap.width(), input_pixmap.height()),
        input_pixmap.rowBytes(),
        /*discard_transparency=*/false,
        /*comments=*/{}, &output);

    base::WriteFile(NextOutputFilePath(), output);
  }

 private:
  // Return path of next output file. Will not handle overflow of file_id_.
  base::FilePath NextOutputFilePath() {
    // maximum possible length of next_file_id_ (in characters)
    constexpr int kPaddedIntLength = 10;

    std::string file_name =
        base::StringPrintf("%0*u.png", kPaddedIntLength, next_file_id_++);

    return output_dir_.Append(base::FilePath::FromUTF8Unsafe(file_name));
  }

  base::FilePath output_dir_;
  uint32_t next_file_id_ = 0;
};

}  // namespace

FuzzerSoftwareOutputSurfaceProvider::FuzzerSoftwareOutputSurfaceProvider(
    std::optional<base::FilePath> png_dir_path)
    : png_dir_path_(png_dir_path) {}

FuzzerSoftwareOutputSurfaceProvider::~FuzzerSoftwareOutputSurfaceProvider() =
    default;

std::unique_ptr<DisplayCompositorMemoryAndTaskController>
FuzzerSoftwareOutputSurfaceProvider::CreateGpuDependency(
    bool gpu_compositing,
    gpu::SurfaceHandle surface_handle) {
  return nullptr;
}

std::unique_ptr<OutputSurface>
FuzzerSoftwareOutputSurfaceProvider::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    DisplayCompositorMemoryAndTaskController* gpu_dependency,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
  std::unique_ptr<SoftwareOutputDevice> software_output_device =
      png_dir_path_ ? std::make_unique<PNGSoftwareOutputDevice>(*png_dir_path_)
                    : std::make_unique<SoftwareOutputDevice>();
  return std::make_unique<SoftwareOutputSurface>(
      std::move(software_output_device));
}

gpu::SharedImageManager*
FuzzerSoftwareOutputSurfaceProvider::GetSharedImageManager() {
  return nullptr;
}

gpu::SyncPointManager*
FuzzerSoftwareOutputSurfaceProvider::GetSyncPointManager() {
  return nullptr;
}

gpu::Scheduler* FuzzerSoftwareOutputSurfaceProvider::GetGpuScheduler() {
  return nullptr;
}
}  // namespace viz
