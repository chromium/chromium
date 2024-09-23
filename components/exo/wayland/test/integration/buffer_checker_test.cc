// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <drm_fourcc.h>
#include <gbm.h>
#include <sys/mman.h>

#include <cstdint>
#include <iterator>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
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

std::string DrmCodeToString(uint32_t drm_format) {
  return std::string{static_cast<char>(drm_format),
                     static_cast<char>(drm_format >> 8),
                     static_cast<char>(drm_format >> 16),
                     static_cast<char>(drm_format >> 24), 0};
}

std::string DrmCodeToBufferFormatString(int32_t drm_format) {
  return gfx::BufferFormatToString(
      ui::GetBufferFormatFromFourCCFormat(drm_format));
}

std::string DrmModifiersToString(std::vector<uint64_t> drm_modifiers) {
  std::stringstream ss;
  for (uint64_t drm_modifier : drm_modifiers)
    ss << "0x" << std::hex << drm_modifier << " ";
  return ss.str();
}

}  // namespace

namespace exo {
namespace wayland {
namespace test {

namespace {

#define WL_ARRAY_FOR_EACH(pos, array, type)                             \
  for (pos = (type)(array)->data;                                       \
       (const char*)pos < ((const char*)(array)->data + (array)->size); \
       (pos)++)

class BufferCheckerTestClient : public ::exo::wayland::clients::ClientBase {
 public:
  explicit BufferCheckerTestClient() = default;
  ~BufferCheckerTestClient() override = default;

  int GetNumSupportedUsages(uint32_t format) {
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
        return -1;
      }

      // Buffers may fail to be created, so loop until we get one or return
      // early if we run out. We can't loop in the outer loop because, without
      // doing the rest of the code, we won't be dispatched to again.
      do {
        if (usages_to_test.size() == 0) {
          std::vector<std::string> supported_usage_strings;
          base::ranges::transform(supported_usages,
                                  std::back_inserter(supported_usage_strings),
                                  gfx::BufferUsageToString);
          LOG(INFO) << "Successfully used buffer with format drm: "
                    << DrmCodeToString(format) << " gfx::BufferFormat: "
                    << DrmCodeToBufferFormatString(format)
                    << " gfx::BufferUsages: ["
                    << base::JoinString(supported_usage_strings, ", ") << "]";
          return supported_usages.size();
        }

        current_usage = usages_to_test.front();
        usages_to_test.pop();

        current_buffer = CreateDrmBuffer(
            gfx::Size(surface_size_.width(), surface_size_.height()), format,
            nullptr, 0, ui::BufferUsageToGbmFlags(current_usage),
            /*y_invert=*/false);
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
    return -1;
  }

  int GetNumSupportedFormatsAndModifier(uint32_t format,
                                        std::vector<uint64_t> modifiers) {
    std::vector<gfx::BufferUsage> supported_usages;
    bool callback_pending = false;
    std::unique_ptr<wl_callback> frame_callback;
    wl_callback_listener frame_listener = {
        [](void* data, struct wl_callback*, uint32_t) {
          *(static_cast<bool*>(data)) = false;
        }};

    std::unique_ptr<Buffer> current_buffer;
    do {
      if (callback_pending)
        continue;

      if (current_buffer) {
        LOG(INFO) << "Successfully used buffer with drm format: "
                  << DrmCodeToString(format)
                  << " drm modifiers: " << DrmModifiersToString(modifiers)
                  << " gfx::BufferFormat: "
                  << DrmCodeToBufferFormatString(format);
        return 1;
      }

      if (wl_display_get_error(display_.get())) {
        LOG(ERROR) << "Wayland error encountered";
        return -1;
      }

      if (modifiers.size() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID) {
        current_buffer = CreateDrmBuffer(
            gfx::Size(surface_size_.width(), surface_size_.height()), format,
            nullptr, 0, ui::BufferUsageToGbmFlags(gfx::BufferUsage::GPU_READ),
            /*y_invert=*/false);
      } else {
        current_buffer = CreateDrmBuffer(
            gfx::Size(surface_size_.width(), surface_size_.height()), format,
            modifiers.data(), modifiers.size(),
            ui::BufferUsageToGbmFlags(gfx::BufferUsage::GPU_READ),
            /*y_invert=*/false);
      }
      if (!current_buffer) {
        LOG(ERROR) << "Unable to create buffer for drm format: "
                   << DrmCodeToString(format)
                   << " drm modifiers: " << DrmModifiersToString(modifiers)
                   << " gfx::BufferFormat: "
                   << DrmCodeToBufferFormatString(format);
        return 0;
      }

      LOG(INFO) << "Attempting to use buffer with format drm format: "
                << DrmCodeToString(format)
                << " drm modifiers: " << DrmModifiersToString(modifiers)
                << " gfx::BufferFormat: "
                << DrmCodeToBufferFormatString(format);

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
    return -1;
  }

  std::vector<uint32_t> reported_formats;
  base::flat_map<uint32_t, std::vector<uint64_t>> reported_format_modifier_map;

  struct WaylandDmabufFeedbackFormat {
    uint32_t format;
    uint32_t padding;
    uint64_t modifier;
  };

  struct DmabufFeedbackTranche {
    dev_t target_device;
    uint32_t flags;
    base::flat_map<uint32_t, std::vector<uint64_t>> format_modifier_map;
  };

  struct DmabufFeedback {
    dev_t main_device;
    std::vector<WaylandDmabufFeedbackFormat> format_table;
    std::vector<DmabufFeedbackTranche> tranches;
    DmabufFeedbackTranche pending_tranche;
  };

  DmabufFeedback current_feedback_;
  DmabufFeedback pending_feedback_;

  void HandleFeedbackDone(zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
    current_feedback_ = pending_feedback_;
    pending_feedback_ = {};

    reported_formats.clear();
    reported_format_modifier_map.clear();
    for (DmabufFeedbackTranche tranche : current_feedback_.tranches) {
      for (const auto& [format, modifiers] : tranche.format_modifier_map) {
        if (!base::Contains(reported_formats, format)) {
          reported_formats.push_back(format);
        }
        if (!reported_format_modifier_map.contains(format)) {
          reported_format_modifier_map[format] = std::vector<uint64_t>();
        }

        for (uint64_t modifier : modifiers) {
          if (!base::Contains(reported_format_modifier_map[format], modifier)) {
            reported_format_modifier_map[format].push_back(modifier);
          }
        }
      }
    }
  }

  void HandleFeedbackFormatTable(
      zwp_linux_dmabuf_feedback_v1* zwp_linux_dmabuf_feedback_v1,
      int32_t fd,
      uint32_t size) {
    ASSERT_TRUE(pending_feedback_.format_table.empty());

    WaylandDmabufFeedbackFormat* format_table =
        static_cast<WaylandDmabufFeedbackFormat*>(
            mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0));
    uint32_t table_size = size / sizeof(WaylandDmabufFeedbackFormat);
    for (uint32_t i = 0; i < table_size; i++) {
      pending_feedback_.format_table.push_back(format_table[i]);
    }
    munmap(format_table, size);
    close(fd);
  }

  void HandleFeedbackMainDevice(zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                                wl_array* dev) {
    memcpy(&pending_feedback_.main_device, dev->data, sizeof(dev));
  }

  void HandleFeedbackTrancheDone(
      zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
    pending_feedback_.tranches.push_back(pending_feedback_.pending_tranche);
    pending_feedback_.pending_tranche = {};
  }

  void HandleFeedbackTrancheTargetDevice(
      zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
      wl_array* dev) {
    memcpy(&pending_feedback_.pending_tranche.target_device, dev->data,
           sizeof(dev));
  }

  void HandleFeedbackTrancheFormats(
      zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
      wl_array* indices) {
    std::vector<WaylandDmabufFeedbackFormat>* format_table =
        &pending_feedback_.format_table;
    if (format_table == nullptr)
      format_table = &current_feedback_.format_table;
    ASSERT_TRUE(format_table != nullptr);

    uint16_t* index;
    WL_ARRAY_FOR_EACH(index, indices, uint16_t*) {
      uint32_t format = format_table->at(*index).format;
      uint64_t modifier = format_table->at(*index).modifier;

      if (!pending_feedback_.pending_tranche.format_modifier_map.contains(
              format))
        pending_feedback_.pending_tranche.format_modifier_map[format] =
            std::vector<uint64_t>();

      pending_feedback_.pending_tranche.format_modifier_map[format].push_back(
          modifier);
    }
  }

  void HandleFeedbackTrancheFlags(zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                                  uint32_t flags) {
    pending_feedback_.pending_tranche.flags = flags;
  }

  void AddFeedbackListener(zwp_linux_dmabuf_feedback_v1* dmabuf_feedback_obj) {
    static struct zwp_linux_dmabuf_feedback_v1_listener kLinuxFeedbackListener =
        {
            .done =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackDone(dmabuf_feedback);
                },
            .format_table =
                [](void* data, zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                   int32_t fd, uint32_t size) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackFormatTable(dmabuf_feedback, fd, size);
                },
            .main_device =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                   struct wl_array* dev) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackMainDevice(dmabuf_feedback, dev);
                },
            .tranche_done =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackTrancheDone(dmabuf_feedback);
                },
            .tranche_target_device =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                   struct wl_array* dev) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackTrancheTargetDevice(dmabuf_feedback, dev);
                },
            .tranche_formats =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                   struct wl_array* indices) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackTrancheFormats(dmabuf_feedback, indices);
                },
            .tranche_flags =
                [](void* data,
                   struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
                   uint32_t flags) {
                  static_cast<BufferCheckerTestClient*>(data)
                      ->HandleFeedbackTrancheFlags(dmabuf_feedback, flags);
                },
        };

    zwp_linux_dmabuf_feedback_v1_add_listener(dmabuf_feedback_obj,
                                              &kLinuxFeedbackListener, this);
    wl_display_roundtrip(display_.get());
  }

  void GetDefaultFeedback() {
    zwp_linux_dmabuf_feedback_v1* dmabuf_feedback_obj =
        zwp_linux_dmabuf_v1_get_default_feedback(globals_.linux_dmabuf.get());

    AddFeedbackListener(dmabuf_feedback_obj);
  }

  void GetSurfaceFeedback() {
    zwp_linux_dmabuf_feedback_v1* dmabuf_feedback_obj =
        zwp_linux_dmabuf_v1_get_surface_feedback(globals_.linux_dmabuf.get(),
                                                 surface_.get());

    AddFeedbackListener(dmabuf_feedback_obj);
  }

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
    if (!reported_format_modifier_map.contains(format)) {
      reported_format_modifier_map[format] = std::vector<uint64_t>();
    }

    uint64_t modifier = static_cast<uint64_t>(modifier_hi) << 32 | modifier_lo;
    reported_format_modifier_map[format].push_back(modifier);
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
  LOG(INFO) << "zwp_linux_dmabuf_v1 reported supported DRM formats: "
             << base::JoinString(drm_names, ", ");
  LOG(INFO) << "zwp_linux_dmabuf_v1 reported supported gfx::BufferFormats: "
             << base::JoinString(buffer_names, ", ");
}

TEST_F(BufferCheckerClientTest, CanUseAnyReportedBufferFormatsLegacy) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version =
      ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));
  EXPECT_TRUE(!client.reported_formats.empty());

  PrintReportedFormats(client.reported_formats);
  bool has_any_supported_formats = false;
  for (auto format : client.reported_formats) {
    int res = client.GetNumSupportedUsages(format);
    EXPECT_TRUE(res != -1);
    if (res > 0) {
      has_any_supported_formats = true;
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}

TEST_F(BufferCheckerClientTest, CanUseAnyReportedBufferModifiersLegacy) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version = ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));
  EXPECT_TRUE(!client.reported_format_modifier_map.empty());

  bool has_any_supported_formats = false;
  for (const auto& [format, modifiers] : client.reported_format_modifier_map) {
    std::vector<uint64_t> valid_modifiers;
    for (uint64_t modifier : modifiers) {
      if (modifier != DRM_FORMAT_MOD_INVALID)
        valid_modifiers.push_back(modifier);
    }

    if (!valid_modifiers.empty()) {
      int res =
          client.GetNumSupportedFormatsAndModifier(format, valid_modifiers);
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }

    if (base::Contains(modifiers, DRM_FORMAT_MOD_INVALID)) {
      int res = client.GetNumSupportedFormatsAndModifier(
          format, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}

TEST_F(BufferCheckerClientTest, CanUseAnyReportedBufferFormatsDefaultFeedback) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version =
      ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));

  EXPECT_TRUE(client.reported_format_modifier_map.empty());
  client.GetDefaultFeedback();
  EXPECT_TRUE(!client.reported_format_modifier_map.empty());

  PrintReportedFormats(client.reported_formats);
  bool has_any_supported_formats = false;
  for (auto format : client.reported_formats) {
    int res = client.GetNumSupportedUsages(format);
    EXPECT_TRUE(res != -1);
    if (res > 0) {
      has_any_supported_formats = true;
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}

TEST_F(BufferCheckerClientTest,
       CanUseAnyReportedBufferModifiersDefaultFeedback) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version =
      ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));

  EXPECT_TRUE(client.reported_format_modifier_map.empty());
  client.GetDefaultFeedback();
  EXPECT_TRUE(!client.reported_format_modifier_map.empty());

  bool has_any_supported_formats = false;
  for (const auto& [format, modifiers] : client.reported_format_modifier_map) {
    std::vector<uint64_t> valid_modifiers;
    for (uint64_t modifier : modifiers) {
      if (modifier != DRM_FORMAT_MOD_INVALID)
        valid_modifiers.push_back(modifier);
    }

    if (!valid_modifiers.empty()) {
      int res =
          client.GetNumSupportedFormatsAndModifier(format, valid_modifiers);
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }

    if (base::Contains(modifiers, DRM_FORMAT_MOD_INVALID)) {
      int res = client.GetNumSupportedFormatsAndModifier(
          format, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}

TEST_F(BufferCheckerClientTest, CanUseAnyReportedBufferFormatsSurfaceFeedback) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version =
      ZWP_LINUX_DMABUF_V1_GET_SURFACE_FEEDBACK_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));

  EXPECT_TRUE(client.reported_format_modifier_map.empty());
  client.GetSurfaceFeedback();
  EXPECT_TRUE(!client.reported_format_modifier_map.empty());

  PrintReportedFormats(client.reported_formats);
  bool has_any_supported_formats = false;
  for (auto format : client.reported_formats) {
    int res = client.GetNumSupportedUsages(format);
    EXPECT_TRUE(res != -1);
    if (res > 0) {
      has_any_supported_formats = true;
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}

TEST_F(BufferCheckerClientTest,
       CanUseAnyReportedBufferModifiersSurfaceFeedback) {
  exo::wayland::test::BufferCheckerTestClient client;
  auto params = base_params_;
  // Initialize no buffers when we start, wait until we've gotten the list
  params.num_buffers = 0;
  params.linux_dmabuf_version =
      ZWP_LINUX_DMABUF_V1_GET_SURFACE_FEEDBACK_SINCE_VERSION;
  ASSERT_TRUE(client.Init(params));

  EXPECT_TRUE(client.reported_format_modifier_map.empty());
  client.GetSurfaceFeedback();
  EXPECT_TRUE(!client.reported_format_modifier_map.empty());

  bool has_any_supported_formats = false;
  for (const auto& [format, modifiers] : client.reported_format_modifier_map) {
    std::vector<uint64_t> valid_modifiers;
    for (uint64_t modifier : modifiers) {
      if (modifier != DRM_FORMAT_MOD_INVALID)
        valid_modifiers.push_back(modifier);
    }

    if (!valid_modifiers.empty()) {
      int res =
          client.GetNumSupportedFormatsAndModifier(format, valid_modifiers);
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }

    if (base::Contains(modifiers, DRM_FORMAT_MOD_INVALID)) {
      int res = client.GetNumSupportedFormatsAndModifier(
          format, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));
      EXPECT_TRUE(res != -1);
      if (res > 0) {
        has_any_supported_formats = true;
      }
    }
  }
  EXPECT_TRUE(has_any_supported_formats);
}
