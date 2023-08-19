// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome-color-management-client-protocol.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <sys/mman.h>

#include <cstdint>
#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

using DrawFunction = base::OnceCallback<void(const gfx::Size&, SkCanvas*)>;

void DrawToGbm(const gfx::Size& size, DrawFunction draw_func, gbm_bo* bo) {
  CHECK_EQ(gbm_bo_get_plane_count(bo), 1);
  uint32_t stride_bytes;
  void* mapped_data;
  void* void_data =
      gbm_bo_map(bo, 0, 0, size.width(), size.height(), GBM_BO_TRANSFER_WRITE,
                 &stride_bytes, &mapped_data);
  CHECK_NE(void_data, MAP_FAILED);
  CHECK_EQ(stride_bytes % 4, 0u);

  auto image_info =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType);
  auto canvas = SkCanvas::MakeRasterDirect(image_info, void_data, stride_bytes);

  canvas->clear(SK_ColorBLACK);

  std::move(draw_func).Run(size, canvas.get());

  gbm_bo_unmap(bo, mapped_data);
}

void DrawColorGradients(const gfx::Size& size, SkCanvas* canvas) {
  constexpr std::array<SkColor, 4> kGradientColors{
      SK_ColorRED,
      SK_ColorBLUE,
      SK_ColorGREEN,
      SK_ColorWHITE,
  };
  int bar_pixels = size.height() / kGradientColors.size();
  for (size_t i = 0; i < kGradientColors.size(); i++) {
    SkPoint points[2] = {SkPoint::Make(0, 0),
                         SkPoint::Make(size.width(), bar_pixels)};
    SkColor colors[2] = {SK_ColorBLACK, kGradientColors[i]};
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(
        points, colors, nullptr, 2, SkTileMode::kClamp, 0, nullptr));

    canvas->drawRect(
        SkRect::MakeXYWH(0, i * bar_pixels, size.width(), bar_pixels), paint);
  }
}

void DrawWhiteBox(const gfx::Size& size, SkCanvas* canvas) {
  SkRect rect = SkRect::MakeXYWH(size.width() / 4, size.height() / 4,
                                 size.width() / 2, size.height() / 2);
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  canvas->drawRect(rect, paint);
}

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

std::string ColorSpaceCreationErrorToString(uint32_t error) {
  switch (error) {
    case ZCR_COLOR_SPACE_CREATOR_V1_CREATION_ERROR_MALFORMED_ICC:
      return "malformed ICC profile";
    case ZCR_COLOR_SPACE_CREATOR_V1_CREATION_ERROR_BAD_ICC:
      return "ICC profile does not meet requirements";
    case ZCR_COLOR_SPACE_CREATOR_V1_CREATION_ERROR_BAD_PRIMARIES:
      return "bad primaries";
    case ZCR_COLOR_SPACE_CREATOR_V1_CREATION_ERROR_BAD_WHITEPOINT:
      return "bad whitepoint";
    default:
      return "<unknown error>";
  }
}

const zcr_color_management_output_v1_listener kOutputColorMangerListener = {
    .color_space_changed =
        [](void* data, struct zcr_color_management_output_v1* color_output) {},
    .extended_dynamic_range =
        [](void* data,
           struct zcr_color_management_output_v1* color_output,
           uint32_t value) {}};

const zcr_color_management_surface_v1_listener kSurfaceColorManagerListener = {
    .preferred_color_space =
        [](void* data,
           struct zcr_color_management_surface_v1* color_surface,
           struct wl_output* output) {}};

}  // namespace

class HdrClient : public ClientBase {
 public:
  HdrClient() = default;

  HdrClient(const HdrClient&) = delete;
  HdrClient& operator=(const HdrClient&) = delete;

  void InitColorManagement();

  void Run(const ClientBase::InitParams& params,
           bool test_white_levels,
           uint32_t primary1,
           uint32_t transfer1,
           uint32_t primary2,
           uint32_t transfer2);

 private:
  std::unique_ptr<zcr_color_space_v1> CreateColorSpace(uint32_t color,
                                                       uint32_t transfer);

  std::unique_ptr<zcr_color_management_output_v1> color_management_output_;
  std::unique_ptr<zcr_color_management_surface_v1> color_management_surface_;
};

void HdrClient::InitColorManagement() {
  CHECK(globals_.color_manager)
      << "Server doesn't support zcr_color_manager_v1.";

  // This is only for the single output scenario.
  color_management_output_.reset(
      zcr_color_manager_v1_get_color_management_output(
          globals_.color_manager.get(), globals_.outputs.back().get()));
  CHECK(color_management_output_) << "Can't create color management output.";
  zcr_color_management_output_v1_add_listener(
      color_management_output_.get(), &kOutputColorMangerListener, this);

  color_management_surface_.reset(
      zcr_color_manager_v1_get_color_management_surface(
          globals_.color_manager.get(), surface_.get()));
  CHECK(color_management_surface_) << "Can't create color management surface.";
  zcr_color_management_surface_v1_add_listener(
      color_management_surface_.get(), &kSurfaceColorManagerListener, this);
}

std::unique_ptr<zcr_color_space_v1> HdrClient::CreateColorSpace(
    uint32_t color,
    uint32_t transfer) {
  std::unique_ptr<zcr_color_space_creator_v1> creator(
      zcr_color_manager_v1_create_color_space_from_names(
          globals_.color_manager.get(), transfer, color,
          ZCR_COLOR_MANAGER_V1_WHITEPOINT_NAMES_D50));

  // Since we're doing a wl_display_roundtrip, we can do all this state
  // management on the stack a clean it up once we get out of scope.
  struct creation_data_t {
    std::unique_ptr<zcr_color_space_v1> color_space = nullptr;
    uint32_t error = 0;
  } creation_data;

  zcr_color_space_creator_v1_listener creator_listener = {
      .created =
          [](void* data,
             struct zcr_color_space_creator_v1* zcr_color_space_creator_v1,
             struct zcr_color_space_v1* new_color_space) {
            static_cast<creation_data_t*>(data)->color_space.reset(
                new_color_space);
          },
      .error =
          [](void* data,
             struct zcr_color_space_creator_v1* zcr_color_space_creator_v1,
             uint32_t error) {
            static_cast<creation_data_t*>(data)->error = error;
          }};
  zcr_color_space_creator_v1_add_listener(creator.get(), &creator_listener,
                                          &creation_data);

  wl_display_roundtrip(display_.get());

  if (creation_data.error) {
    LOG(FATAL) << "Unable to create colorspace for primaries=" << color
               << " transfer=" << transfer << " reason='"
               << ColorSpaceCreationErrorToString(creation_data.error) << "'";
  }

  return std::move(creation_data.color_space);
}

void HdrClient::Run(const ClientBase::InitParams& params,
                    bool test_white_levels,
                    uint32_t primary1,
                    uint32_t transfer1,
                    uint32_t primary2,
                    uint32_t transfer2) {
  if (!ClientBase::Init(params))
    return;

  InitColorManagement();

  std::vector<std::unique_ptr<zcr_color_space_v1>> color_spaces;
  color_spaces.push_back(CreateColorSpace(primary1, transfer1));
  color_spaces.push_back(CreateColorSpace(primary2, transfer2));

  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  for (auto& buff : buffers_) {
    if (test_white_levels) {
      DrawToGbm(size_, base::BindOnce(DrawWhiteBox), buff->bo.get());
    } else {
      DrawToGbm(size_, base::BindOnce(DrawColorGradients), buff->bo.get());
    }
  }

  constexpr int kFramesPerCycle = 60;
  size_t frame_number = 0;
  size_t color_space_idx = 0;
  do {
    if (callback_pending)
      continue;

    Buffer* buffer = DequeueBuffer();
    if (!buffer) {
      LOG(ERROR) << "Can't find free buffer";
      return;
    }

    frame_number++;
    if (frame_number % kFramesPerCycle == 0) {
      color_space_idx = (color_space_idx + 1) % color_spaces.size();
      LOG(ERROR) << "Switching to color space "
                 << (color_space_idx == 0 ? "PRIMARY" : "SECONDARY");
    }

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    zcr_color_management_surface_v1_set_color_space(
        color_management_surface_.get(), color_spaces[color_space_idx].get(),
        ZCR_COLOR_MANAGEMENT_SURFACE_V1_RENDER_INTENT_PERCEPTUAL);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
    callback_pending = true;
    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  } while (wl_display_dispatch(display_.get()) != -1);
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  exo::wayland::clients::ClientBase::InitParams params;
  params.num_buffers = 8;
  if (!params.FromCommandLine(*command_line))
    return 1;
  CHECK(params.use_drm) << "Missing --use-drm parameter which is required for "
                           "gbm buffer allocation";

  // sRGB
  int32_t primary1 = ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT709;
  int32_t transfer1 = ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB;
  // HDR BT2020, PQ, full range RGB
  int32_t primary2 = ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT2020;
  int32_t transfer2 = ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ;

  params.bo_usage =
      GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;

  exo::wayland::clients::HdrClient client;
  client.Run(params, command_line->HasSwitch("white-levels"), primary1,
             transfer1, primary2, transfer2);
  return 0;
}
