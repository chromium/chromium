// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gbm.h>
#include <cstdint>
#include <iterator>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_util.h"

namespace {

std::string DrmCodeToString(uint64_t drm_format) {
  return std::string{static_cast<char>(drm_format),
                     static_cast<char>(drm_format >> 8),
                     static_cast<char>(drm_format >> 16),
                     static_cast<char>(drm_format >> 24), 0};
}

std::string DrmCodeToBufferFormatString(uint64_t drm_format) {
  return gfx::BufferFormatToString(
      ui::GetBufferFormatFromFourCCFormat(drm_format));
}

}  // namespace

namespace exo {
namespace wayland {
namespace test {

namespace {

class BufferCheckerTestClient : public ::exo::wayland::clients::ClientBase {
 public:
  explicit BufferCheckerTestClient() = default;
  ~BufferCheckerTestClient() override = default;

  bool HasAnySupportedUsages(uint32_t format) {
    std::vector<gfx::BufferUsage> supported_usages;
    bool callback_pending = false;
    std::unique_ptr<wl_callback> frame_callback;
    wl_callback_listener frame_listener = {
        [](void* data, struct wl_callback*, uint32_t) {
          *(static_cast<bool*>(data)) = false;
        }};

    base::queue<gfx::BufferUsage> usages_to_test;
    for (int i = 0; i < static_cast<int>(gfx::BufferUsage::LAST); i++)
      usages_to_test.push(static_cast<gfx::BufferUsage>(i));

    gfx::BufferUsage current_usage;
    std::unique_ptr<Buffer> current_buffer;
    do {
      if (callback_pending)
        continue;

      if (current_buffer) {
        supported_usages.push_back(current_usage);
      }

      if (wl_display_get_error(display_.get())) {
        LOG(ERROR) << "Wayland error encountered";
        return false;
      }

      // Buffers may fail to be created, so loop until we get one or return
      // early if we run out. We can't loop in the outer loop because, without
      // doing the rest of the code, we won't be dispatched to again.
      do {
        if (usages_to_test.size() == 0) {
          std::vector<std::string> supported_usage_strings;
          std::transform(supported_usages.begin(), supported_usages.end(),
                         std::back_inserter(supported_usage_strings),
                         gfx::BufferUsageToString);
          LOG(INFO) << "Successfully used buffer with format drm: "
                    << DrmCodeToString(format) << " gfx::BufferFormat: "
                    << DrmCodeToBufferFormatString(format)
                    << " gfx::BufferUsages: ["
                    << base::JoinString(supported_usage_strings, ", ") << "]";
          return supported_usages.size() > 0;
        }

        current_usage = usages_to_test.front();
        usages_to_test.pop();

        current_buffer = CreateDrmBuffer(
            gfx::Size(surface_size_.width(), surface_size_.height()), format,
            ui::BufferUsageToGbmFlags(current_usage), /*y_invert=*/false);
        if (!current_buffer) {
          LOG(ERROR) << "Unable to create buffer for drm: "
                     << DrmCodeToString(format) << " gfx::BufferFormat: "
                     << DrmCodeToBufferFormatString(format)
                     << " gfx::BufferUsage "
                     << gfx::BufferUsageToString(current_usage);
        }
      } while (current_buffer == nullptr);

      LOG(INFO) << "Attempting to use buffer with format drm: "
                << DrmCodeToString(format)
                << " gfx::BufferFormat: " << DrmCodeToBufferFormatString(format)
                << " gfx::BufferUsage "
                << gfx::BufferUsageToString(current_usage);
      ;

      wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                        surface_size_.height());
      wl_surface_attach(surface_.get(), current_buffer->buffer.get(), 0, 0);

      frame_callback.reset(wl_surface_frame(surface_.get()));
      wl_callback_add_listener(frame_callback.get(), &frame_listener,
                               &callback_pending);
      callback_pending = true;
      wl_surface_commit(surface_.get());

      wl_display_flush(display_.get());
    } while (wl_display_dispatch(display_.get()) != -1);

    LOG(ERROR)
        << "Expected to return from inside the loop. Wayland disconnected?";
    return false;
  }

  std::vector<uint32_t> reported_formats;
  base::flat_map<uint32_t, std::vector<uint64_t>> reported_format_modifer_map;

 protected:
  void HandleDmabufFormat(void* data,
                          struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                          uint32_t format) override {
    reported_formats.push_back(format);
  }

  void HandleDmabufModifier(void* data,
                            struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                            uint32_t format,
                            uint32_t modifier_hi,
                            uint32_t modifier_lo) override {
    if (!reported_format_modifer_map.contains(format)) {
      reported_format_modifer_map[format] = std::vector<uint64_t>();
    }

    uint64_t modifier = static_cast<uint64_t>(modifier_hi) << 32 | modifier_lo;
    reported_format_modifer_map[format].push_back(modifier);
  }
};

}  // namespace
}  // namespace test
}  // namespace wayland
}  // namespace exo

class BufferCheckerClientTest : public testing::Test {
 protected:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    CHECK(base_params_.FromCommandLine(*command_line));
    CHECK(base_params_.use_drm) << "Missing --use-drm parameter which is "
                                   "required for gbm buffer allocation";
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  exo::wayland::clients::ClientBase::InitParams base_params_;
};

void PrintReportedFormats(std::vector<uint32_t>& formats) {
  std::vector<std::string> drm_names;
  std::vector<std::string> buffer_names;
  for (auto format : formats) {
    drm_names.push_back(DrmCodeToString(format));
    buffer_names.push_back(DrmCodeToBufferFormatString(format));
  }
  LOG(ERROR) << "zwp_linux_dmabuf_v1 reported supported DRM formats: "
             << base::JoinString(drm_names, ", ");
  LOG(ERROR) << "zwp_linux_dmabuf_v1 reported supported gfx::BufferFormats: "
             << base::JoinString(buffer_names, ", ");
}

TEST_F(BufferCheckerClientTest, CanUseAllReportedBuffers) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  ASSERT_TRUE(client.Init(params));
  PrintReportedFormats(client.reported_formats);
  for (auto format : client.reported_formats)
    EXPECT_TRUE(client.HasAnySupportedUsages(format));
}
