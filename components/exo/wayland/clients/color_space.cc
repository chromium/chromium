// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <sys/mman.h>

#include <color-space-unstable-v1-client-protocol.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

void WriteMixedPrimaries(gbm_bo* bo, const gfx::Size& size) {
  CHECK_EQ(gbm_bo_get_plane_count(bo), 1u);
  uint32_t stride;
  void* mapped_data;
  void* void_data = gbm_bo_map(bo, 0, 0, size.width(), size.height(),
                               GBM_BO_TRANSFER_WRITE, &stride, &mapped_data, 0);
  CHECK_NE(void_data, MAP_FAILED);
  uint32_t* data = static_cast<uint32_t*>(void_data);
  CHECK_EQ(stride % 4, 0u);
  stride = stride / 4;
  // Draw rows that interpolate from R->G->B->R so we draw the outside edges of
  // the color gamut we are using and than ramp it from 0-255 going left to
  // right.
  int h1 = size.height() / 3;
  int h2 = 2 * h1;
  int h_rem = size.height() - h2;
  int xscaler = size.width() - 1;
  for (int y = 0; y < h1; ++y) {
    for (int x = 0; x < size.width(); ++x) {
      SkColor c = SkColorSetRGB(x * 0xFF * (h1 - y) / (h1 * xscaler),
                                x * 0xFF * y / (h1 * xscaler), 0);
      data[stride * y + x] = c;
    }
  }
  for (int y = h1; y < h2; ++y) {
    for (int x = 0; x < size.width(); ++x) {
      SkColor c = SkColorSetRGB(0, x * 0xFF * (h2 - y) / (h1 * xscaler),
                                x * 0xFF * (y - h1) / (h1 * xscaler));
      data[stride * y + x] = c;
    }
  }
  for (int y = h2; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      SkColor c =
          SkColorSetRGB(x * 0xFF * (y - h2) / (h_rem * xscaler), 0,
                        x * 0xFF * (size.height() - y) / (h_rem * xscaler));
      data[stride * y + x] = c;
    }
  }
  gbm_bo_unmap(bo, mapped_data);
}

}  // namespace

// Wayland client implementation which renders a gradient to a surface and
// switches between two different color spaces every 30 frames. There are
// command line options available to switch all attributes of each color space
// used. This is for testing setting color spaces on wayland surfaces.
class ColorSpaceClient : public ClientBase {
 public:
  ColorSpaceClient() {}

  void Run(const ClientBase::InitParams& params,
           uint32_t primary1,
           uint32_t transfer1,
           uint32_t matrix1,
           uint32_t range1,
           uint32_t primary2,
           uint32_t transfer2,
           uint32_t matrix2,
           uint32_t range2);
};

void ColorSpaceClient::Run(const ClientBase::InitParams& params,
                           uint32_t primary1,
                           uint32_t transfer1,
                           uint32_t matrix1,
                           uint32_t range1,
                           uint32_t primary2,
                           uint32_t transfer2,
                           uint32_t matrix2,
                           uint32_t range2) {
  if (!ClientBase::Init(params))
    return;

  // The server needs to support the color space protocol.
  CHECK(globals_.color_space) << "Server doesn't support zcr_color_space_v1.";

  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  constexpr int kFramesPerCycle = 60;
  size_t frame_number = 0;
  for (auto& buff : buffers_) {
    WriteMixedPrimaries(buff->bo.get(), size_);
  }
  do {
    if (callback_pending)
      continue;
    frame_number++;

    Buffer* buffer = DequeueBuffer();
    if (!buffer) {
      LOG(ERROR) << "Can't find free buffer";
      return;
    }
    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);
    if ((frame_number % kFramesPerCycle) < kFramesPerCycle / 2) {
      if (frame_number % kFramesPerCycle == 0)
        LOG(WARNING) << "Primary color space";
      zcr_color_space_v1_set_color_space(globals_.color_space.get(),
                                         surface_.get(), primary1, transfer1,
                                         matrix1, range1);

    } else {
      if (frame_number % kFramesPerCycle == kFramesPerCycle / 2)
        LOG(WARNING) << "Secondary color space";
      zcr_color_space_v1_set_color_space(globals_.color_space.get(),
                                         surface_.get(), primary2, transfer2,
                                         matrix2, range2);
    }
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

  exo::wayland::clients::ClientBase::InitParams params;
  params.num_buffers = 4;
  if (!params.FromCommandLine(*command_line))
    return 1;

  // sRGB
  int32_t primary1 = ZCR_COLOR_SPACE_V1_PRIMARIES_BT709;
  int32_t transfer1 = ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_IEC61966_2_1;
  int32_t matrix1 = ZCR_COLOR_SPACE_V1_MATRIX_RGB;
  int32_t range1 = ZCR_COLOR_SPACE_V1_RANGE_FULL;
  // HDR BT2020, PQ, full range RGB
  int32_t primary2 = ZCR_COLOR_SPACE_V1_PRIMARIES_BT2020;
  int32_t transfer2 = ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTEST2084;
  int32_t matrix2 = ZCR_COLOR_SPACE_V1_MATRIX_RGB;
  int32_t range2 = ZCR_COLOR_SPACE_V1_RANGE_FULL;
  // Use the values from the enums in the wayland protocol. For example to get
  // the same behavior as default the flags would be:
  // --cs1=0,12,0,1 --cs2=6,15,0,1
  if (command_line->HasSwitch("cs1")) {
    std::vector<std::string> cs1_values =
        base::SplitString(command_line->GetSwitchValueASCII("cs1"), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (cs1_values.size() != 4) {
      LOG(ERROR) << "Invalid value for cs1, needs 4 tokens";
      return 1;
    }
    if (!base::StringToInt(cs1_values[0], &primary1)) {
      LOG(ERROR) << "Invalid value for primary in cs1";
      return 1;
    }
    if (!base::StringToInt(cs1_values[1], &transfer1)) {
      LOG(ERROR) << "Invalid value for transfer function in cs1";
      return 1;
    }
    if (!base::StringToInt(cs1_values[2], &matrix1)) {
      LOG(ERROR) << "Invalid value for matrix in cs1";
      return 1;
    }
    if (!base::StringToInt(cs1_values[3], &range1)) {
      LOG(ERROR) << "Invalid value for range in cs1";
      return 1;
    }
  }
  if (command_line->HasSwitch("cs2")) {
    std::vector<std::string> cs2_values =
        base::SplitString(command_line->GetSwitchValueASCII("cs2"), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (cs2_values.size() != 4) {
      LOG(ERROR) << "Invalid value for cs2, needs 4 tokens";
      return 1;
    }
    if (!base::StringToInt(cs2_values[0], &primary2)) {
      LOG(ERROR) << "Invalid value for primary in cs2";
      return 1;
    }
    if (!base::StringToInt(cs2_values[1], &transfer2)) {
      LOG(ERROR) << "Invalid value for transfer function in cs2";
      return 1;
    }
    if (!base::StringToInt(cs2_values[2], &matrix2)) {
      LOG(ERROR) << "Invalid value for matrix in cs2";
      return 1;
    }
    if (!base::StringToInt(cs2_values[3], &range2)) {
      LOG(ERROR) << "Invalid value for range in cs2";
      return 1;
    }
  }

  if (!params.use_drm) {
    LOG(ERROR) << "Missing --use-drm parameter which is required for gbm "
                 "buffer allocation";
    return 1;
  }

  params.drm_format = DRM_FORMAT_ARGB8888;
  params.bo_usage =
      GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::ColorSpaceClient client;
  client.Run(params, primary1, transfer1, matrix1, range1, primary2, transfer2,
             matrix2, range2);
  return 0;
}
