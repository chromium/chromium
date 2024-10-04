// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/chromeos_camera/mjpeg_decode_accelerator.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/local_gpu_memory_buffer_manager.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/parsers/jpeg_parser.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap_handle.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace chromeos_camera {
namespace {

// Default test image file.
const base::FilePath::CharType* kDefaultJpegFilename =
    FILE_PATH_LITERAL("peach_pi-1280x720.jpg");
constexpr int kDefaultPerfDecodeTimes = 600;
// Decide to save decode results to files or not. Output files will be saved
// in the same directory with unittest. File name is like input file but
// changing the extension to "yuv".
bool g_save_to_file = false;
// Threshold for mean absolute difference of hardware and software decode.
// Absolute difference is to calculate the difference between each pixel in two
// images. This is used for measuring of the similarity of two images.
constexpr double kDecodeSimilarityThreshold = 1.25;

// The buffer usage used to create GpuMemoryBuffer for testing.
constexpr gfx::BufferUsage kBufferUsage =
    gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

// Environment to create test data for all test cases.
class MjpegDecodeAcceleratorTestEnvironment;
MjpegDecodeAcceleratorTestEnvironment* g_env;

// This struct holds a parsed, complete JPEG blob. It can be created from a
// FilePath or can be simply a black image.
struct ParsedJpegImage {
  static std::unique_ptr<ParsedJpegImage> CreateFromFile(
      const base::FilePath& file_path) {
    auto image = std::make_unique<ParsedJpegImage>(file_path);

    LOG_ASSERT(base::ReadFileToString(file_path, &image->data_str))
        << file_path;

    media::JpegParseResult parse_result;
    LOG_ASSERT(
        ParseJpegPicture(base::as_byte_span(image->data_str), &parse_result));

    image->InitializeSizes(parse_result.frame_header.visible_width,
                           parse_result.frame_header.visible_height);
    return image;
  }

  static std::unique_ptr<ParsedJpegImage> CreateBlackImage(
      int width,
      int height,
      SkJpegEncoder::Downsample downsample = SkJpegEncoder::Downsample::k420) {
    // Generate a black image with the specified resolution.
    constexpr size_t kBytesPerPixel = 4;
    const std::vector<unsigned char> input_buffer(width * height *
                                                  kBytesPerPixel);
    const SkImageInfo info = SkImageInfo::Make(
        width, height, kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
    const SkPixmap src(info, input_buffer.data(), width * kBytesPerPixel);

    // Encode the generated image in the JPEG format, the output buffer will be
    // automatically resized while encoding.
    constexpr int kJpegQuality = 100;
    std::vector<unsigned char> encoded;
    LOG_ASSERT(gfx::JPEGCodec::Encode(src, kJpegQuality, downsample, &encoded));

    base::FilePath filename;
    LOG_ASSERT(base::GetTempDir(&filename));
    filename =
        filename.Append(base::StringPrintf("black-%dx%d.jpg", width, height));

    auto image = std::make_unique<ParsedJpegImage>(filename);
    image->data_str.append(encoded.begin(), encoded.end());
    image->InitializeSizes(width, height);
    return image;
  }

  explicit ParsedJpegImage(const base::FilePath& path) : file_path(path) {}

  void InitializeSizes(int width, int height) {
    visible_size.SetSize(width, height);
    // We don't expect odd dimensions for camera captures.
    ASSERT_EQ(0, width % 2);
    ASSERT_EQ(0, height % 2);

    coded_size.SetSize((visible_size.width() + 1) & ~1,
                       (visible_size.height() + 1) & ~1);
  }

  const base::FilePath::StringType& filename() const {
    return file_path.value();
  }

  const base::FilePath file_path;

  std::string data_str;
  gfx::Size visible_size;
  gfx::Size coded_size;
};

// Global singleton to hold on to common data and other user-defined options.
class MjpegDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  MjpegDecodeAcceleratorTestEnvironment(
      const base::FilePath::CharType* jpeg_filenames,
      const base::FilePath::CharType* test_data_path,
      const base::FilePath::CharType* perf_output_path,
      int perf_decode_times)
      : perf_decode_times_(perf_decode_times ? perf_decode_times
                                             : kDefaultPerfDecodeTimes),
        user_jpeg_filenames_(jpeg_filenames ? jpeg_filenames
                                            : kDefaultJpegFilename),
        test_data_path_(test_data_path),
        perf_output_path_(perf_output_path) {}

  void SetUp() override;

  void TearDown() override;

  // Resolve the specified file path. The file path can be either an absolute
  // path, relative to the current directory, or relative to the test data path.
  // This is either a custom test data path provided by --test_data_path, or the
  // default test data path (//media/test/data).
  base::FilePath GetOriginalOrTestDataFilePath(const std::string& file_path) {
    const base::FilePath original_file_path = base::FilePath(file_path);
    if (base::PathExists(original_file_path))
      return original_file_path;
    if (test_data_path_)
      return base::FilePath(test_data_path_).Append(original_file_path);
    return media::GetTestDataFilePath(file_path);
  }

  // Creates a zero-initialized memory VideoFrame.
  scoped_refptr<media::VideoFrame> CreateMemoryVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Size& visible_size);

  // Creates a zero-initialized DMA-buf backed VideoFrame. Also returns the
  // backing GpuMemoryBuffer in |backing_gmb| if it is not null.
  scoped_refptr<media::VideoFrame> CreateDmaBufVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Size& visible_size,
      std::unique_ptr<gfx::GpuMemoryBuffer>* backing_gmb = nullptr);

  // Maps |gmb| into a VideoFrame containing the data pointers. |gmb| should
  // outlive the returned Videoframe.
  scoped_refptr<media::VideoFrame> MapToVideoFrame(
      gfx::GpuMemoryBuffer* gmb,
      const media::VideoFrameLayout& layout,
      const gfx::Rect& visible_rect);

  // Creates a DMA buffer file descriptor that contains |size| bytes of linear
  // data initialized with |data|.
  base::ScopedFD CreateDmaBufFd(const void* data, size_t size);

  // Gets a list of supported DMA-buf frame formats for
  // CreateDmaBufVideoFrame().
  std::vector<media::VideoPixelFormat> GetSupportedDmaBufFormats();

  void AddMetric(const std::string& name, const base::TimeDelta& time);

  // Used for InputSizeChange test case. The image size should be smaller than
  // |kDefaultJpegFilename|.
  std::unique_ptr<ParsedJpegImage> image_data_1280x720_black_;
  // Used for ResolutionChange test case.
  std::unique_ptr<ParsedJpegImage> image_data_640x368_black_;
  // Used for testing some drivers which will align the output resolution to a
  // multiple of 16. 640x360 will be aligned to 640x368.
  std::unique_ptr<ParsedJpegImage> image_data_640x360_black_;
  // Generated black image used to test different JPEG sampling formats.
  std::unique_ptr<ParsedJpegImage> image_data_640x368_422_black_;
  // Parsed data of "peach_pi-1280x720.jpg".
  std::unique_ptr<ParsedJpegImage> image_data_1280x720_default_;
  // Parsed data of failure image.
  std::unique_ptr<ParsedJpegImage> image_data_invalid_;
  // Parsed data from command line.
  std::vector<std::unique_ptr<ParsedJpegImage>> image_data_user_;
  // Decode times for performance measurement.
  int perf_decode_times_;

 private:
  const base::FilePath::CharType* user_jpeg_filenames_;
  const base::FilePath::CharType* test_data_path_;
  const base::FilePath::CharType* perf_output_path_;
  base::Value::Dict metrics_;

  std::unique_ptr<media::LocalGpuMemoryBufferManager>
      gpu_memory_buffer_manager_;
};

void MjpegDecodeAcceleratorTestEnvironment::SetUp() {
  image_data_1280x720_black_ = ParsedJpegImage::CreateBlackImage(1280, 720);
  image_data_640x368_black_ = ParsedJpegImage::CreateBlackImage(640, 368);
  image_data_640x360_black_ = ParsedJpegImage::CreateBlackImage(640, 360);
  image_data_640x368_422_black_ = ParsedJpegImage::CreateBlackImage(
      640, 368, SkJpegEncoder::Downsample::k422);

  image_data_1280x720_default_ = ParsedJpegImage::CreateFromFile(
      GetOriginalOrTestDataFilePath(kDefaultJpegFilename));

  image_data_invalid_ =
      std::make_unique<ParsedJpegImage>(base::FilePath("failure.jpg"));
  image_data_invalid_->data_str.resize(100, 0);
  image_data_invalid_->InitializeSizes(1280, 720);

  // |user_jpeg_filenames_| may include many files and use ';' as delimiter.
  std::vector<base::FilePath::StringType> filenames = base::SplitString(
      user_jpeg_filenames_, base::FilePath::StringType(1, ';'),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& filename : filenames) {
    const base::FilePath input_file = GetOriginalOrTestDataFilePath(filename);
    auto image_data = ParsedJpegImage::CreateFromFile(input_file);
    image_data_user_.push_back(std::move(image_data));
  }

  gpu_memory_buffer_manager_ =
      std::make_unique<media::LocalGpuMemoryBufferManager>();
}

void MjpegDecodeAcceleratorTestEnvironment::TearDown() {
  // Write recorded metrics to file in JSON format.
  if (perf_output_path_ != nullptr) {
    std::string json;
    ASSERT_TRUE(base::JSONWriter::WriteWithOptions(
        metrics_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
    ASSERT_TRUE(base::WriteFile(base::FilePath(perf_output_path_), json));
  }
}

scoped_refptr<media::VideoFrame>
MjpegDecodeAcceleratorTestEnvironment::CreateMemoryVideoFrame(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Size& visible_size) {
  return media::VideoFrame::CreateZeroInitializedFrame(
      format, coded_size, gfx::Rect(visible_size), visible_size,
      base::TimeDelta());
}

scoped_refptr<media::VideoFrame>
MjpegDecodeAcceleratorTestEnvironment::CreateDmaBufVideoFrame(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Size& visible_size,
    std::unique_ptr<gfx::GpuMemoryBuffer>* backing_gmb) {
  DCHECK(gpu_memory_buffer_manager_);

  // Create a GpuMemoryBuffer and get a NativePixmapHandle from it.
  const std::optional<gfx::BufferFormat> gfx_format =
      media::VideoPixelFormatToGfxBufferFormat(format);
  if (!gfx_format) {
    LOG(ERROR) << "Unsupported pixel format: " << format;
    return nullptr;
  }
  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          coded_size, *gfx_format, kBufferUsage, gpu::kNullSurfaceHandle,
          nullptr);
  if (!gmb) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer";
    return nullptr;
  }
  gfx::GpuMemoryBufferHandle gmb_handle = gmb->CloneHandle();
  if (gmb_handle.type != gfx::NATIVE_PIXMAP) {
    LOG(ERROR) << "The GpuMemoryBufferHandle doesn't have type NATIVE_PIXMAP";
    return nullptr;
  }

  const size_t num_planes = media::VideoFrame::NumPlanes(format);
  if (gmb_handle.native_pixmap_handle.planes.size() != num_planes) {
    LOG(ERROR) << "The number of planes of NativePixmapHandle doesn't match "
                  "the pixel format";
    return nullptr;
  }

  // Fill in the memory with zeros.
  if (!gmb->Map()) {
    LOG(ERROR) << "Failed to map GpuMemoryBuffer";
    return nullptr;
  }
  for (size_t i = 0; i < num_planes; i++) {
    gfx::NativePixmapPlane& plane = gmb_handle.native_pixmap_handle.planes[i];
    memset(gmb->memory(i), 0, plane.size);
  }
  gmb->Unmap();

  // Create a VideoFrame from the NativePixmapHandle.
  std::vector<media::ColorPlaneLayout> planes;
  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; i++) {
    gfx::NativePixmapPlane& plane = gmb_handle.native_pixmap_handle.planes[i];
    planes.emplace_back(base::checked_cast<int32_t>(plane.stride),
                        base::checked_cast<size_t>(plane.offset),
                        base::checked_cast<size_t>(plane.size));
    dmabuf_fds.push_back(std::move(plane.fd));
  }
  const std::optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(
          format, coded_size, std::move(planes),
          media::VideoFrameLayout::kBufferAddressAlignment,
          gmb_handle.native_pixmap_handle.modifier);
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  if (backing_gmb)
    *backing_gmb = std::move(gmb);

  return media::VideoFrame::WrapExternalDmabufs(
      *layout, gfx::Rect(visible_size), visible_size, std::move(dmabuf_fds),
      base::TimeDelta());
}

scoped_refptr<media::VideoFrame>
MjpegDecodeAcceleratorTestEnvironment::MapToVideoFrame(
    gfx::GpuMemoryBuffer* gmb,
    const media::VideoFrameLayout& layout,
    const gfx::Rect& visible_rect) {
  DCHECK(gmb);
  if (!gmb->Map()) {
    LOG(ERROR) << "Failed to map GpuMemoryBuffer";
    return nullptr;
  }
  std::array<uint8_t*, 3> data{};
  for (size_t i = 0; i < layout.num_planes(); i++)
    data[i] = static_cast<uint8_t*>(gmb->memory(i));
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalYuvDataWithLayout(
          layout, visible_rect, visible_rect.size(), data[0], data[1], data[2],
          base::TimeDelta());
  if (!frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }
  frame->AddDestructionObserver(
      base::BindOnce(&gfx::GpuMemoryBuffer::Unmap, base::Unretained(gmb)));
  return frame;
}

base::ScopedFD MjpegDecodeAcceleratorTestEnvironment::CreateDmaBufFd(
    const void* data,
    size_t size) {
  DCHECK(data);
  DCHECK_GT(size, 0u);
  DCHECK(gpu_memory_buffer_manager_);

  // The DMA-buf FD is intended to allow importing into hardware accelerators,
  // so we allocate the buffer by GMB manager instead of simply memfd_create().
  // The GMB has R_8 format and dimensions (|size|, 1).
  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          gfx::Size(base::checked_cast<int>(size), 1), gfx::BufferFormat::R_8,
          kBufferUsage, gpu::kNullSurfaceHandle, nullptr);
  if (!gmb) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer";
    return base::ScopedFD();
  }

  gfx::GpuMemoryBufferHandle gmb_handle = gmb->CloneHandle();
  if (gmb_handle.type != gfx::NATIVE_PIXMAP) {
    LOG(ERROR) << "The GpuMemoryBufferHandle doesn't have type NATIVE_PIXMAP";
    return base::ScopedFD();
  }
  if (gmb_handle.native_pixmap_handle.planes.size() != 1) {
    LOG(ERROR) << "The number of planes of NativePixmapHandle is not 1 for R_8 "
                  "format";
    return base::ScopedFD();
  }
  if (gmb_handle.native_pixmap_handle.planes[0].offset != 0) {
    LOG(ERROR) << "The memory offset is not zero";
    return base::ScopedFD();
  }

  // Fill in the memory with |data|.
  if (!gmb->Map()) {
    LOG(ERROR) << "Failed to map GpuMemoryBuffer";
    return base::ScopedFD();
  }
  memcpy(gmb->memory(0), data, size);
  gmb->Unmap();

  return std::move(gmb_handle.native_pixmap_handle.planes[0].fd);
}

std::vector<media::VideoPixelFormat>
MjpegDecodeAcceleratorTestEnvironment::GetSupportedDmaBufFormats() {
  constexpr media::VideoPixelFormat kPreferredFormats[] = {
      media::PIXEL_FORMAT_NV12,
      media::PIXEL_FORMAT_YV12,
  };
  std::vector<media::VideoPixelFormat> supported_formats;
  for (const media::VideoPixelFormat format : kPreferredFormats) {
    const std::optional<gfx::BufferFormat> gfx_format =
        media::VideoPixelFormatToGfxBufferFormat(format);
    if (gfx_format && gpu_memory_buffer_manager_->IsFormatAndUsageSupported(
                          *gfx_format, kBufferUsage))
      supported_formats.push_back(format);
  }
  return supported_formats;
}

void MjpegDecodeAcceleratorTestEnvironment::AddMetric(
    const std::string& name,
    const base::TimeDelta& time) {
  metrics_.Set(name, time.InMillisecondsF());
}

enum ClientState {
  CS_CREATED,
  CS_INITIALIZED,
  CS_DECODE_PASS,
  CS_ERROR,
};

struct DecodeTask {
  raw_ptr<const ParsedJpegImage> image;
  gfx::Size target_size;

  DecodeTask(const ParsedJpegImage* im)
      : image(im), target_size(im->visible_size) {}
  DecodeTask(const ParsedJpegImage* im, const gfx::Size ts)
      : image(im), target_size(ts) {}
};

struct PerfMetrics {
  size_t num_frames_decoded;
  base::TimeDelta total_decode_time;
  base::TimeDelta total_decode_map_time;
};

class JpegClient : public MjpegDecodeAccelerator::Client {
 public:
  // JpegClient takes ownership of |note|.
  JpegClient(
      const std::vector<DecodeTask>& tasks,
      std::unique_ptr<media::test::ClientStateNotification<ClientState>> note,
      bool use_dmabuf,
      bool skip_result_checking);

  JpegClient(const JpegClient&) = delete;
  JpegClient& operator=(const JpegClient&) = delete;

  ~JpegClient() override;
  void CreateJpegDecoder();
  void StartDecode(int32_t task_id, bool do_prepare_memory);
  void PrepareMemory(int32_t task_id);
  bool GetSoftwareDecodeResult(int32_t task_id);
  PerfMetrics GetPerfMetrics() const;

  // MjpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t task_id) override;
  void NotifyError(int32_t task_id,
                   MjpegDecodeAccelerator::Error error) override;

  // Accessors.
  media::test::ClientStateNotification<ClientState>* note() const {
    return note_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(JpegClientTest, GetMeanAbsoluteDifference);

  void SetState(ClientState new_state);
  void OnInitialize(bool initialize_result);

  // Save a video frame that contains a decoded JPEG. The output is a PNG file.
  // The suffix will be added before the .png extension.
  void SaveToFile(int32_t task_id,
                  scoped_refptr<media::VideoFrame> in_frame,
                  const std::string& suffix = "");

  // Calculate mean absolute difference of hardware and software decode results
  // to check the similarity.
  double GetMeanAbsoluteDifference();

  // JpegClient doesn't own |tasks_|.
  const raw_ref<const std::vector<DecodeTask>> tasks_;

  ClientState state_;

  // Used to notify another thread about the state. JpegClient owns this.
  std::unique_ptr<media::test::ClientStateNotification<ClientState>> note_;

  // Use DMA-buf backed output buffer for hardware decoder.
  bool use_dmabuf_;

  // Skip JDA decode result. Used for testing performance.
  bool skip_result_checking_;

  // Input shared memory and mapping.
  base::UnsafeSharedMemoryRegion in_shm_;
  base::WritableSharedMemoryMapping in_shm_mapping_;
  // Input DMA buffer file descriptor.
  base::ScopedFD in_dmabuf_fd_;
  // Output video frame from the hardware decoder.
  std::unique_ptr<gfx::GpuMemoryBuffer> hw_out_gmb_;
  scoped_refptr<media::VideoFrame> hw_out_dmabuf_frame_;
  scoped_refptr<media::VideoFrame> hw_out_frame_;
  // Output and intermediate frame for the software decoder.
  scoped_refptr<media::VideoFrame> sw_out_frame_;
  scoped_refptr<media::VideoFrame> sw_tmp_frame_;

  // This should be the first member to get destroyed because |decoder_|
  // potentially uses other members in the JpegClient instance. For example,
  // as decode tasks finish in a new thread spawned by |decoder_|,
  // |hw_out_frame_| can be accessed.
  std::unique_ptr<MjpegDecodeAccelerator> decoder_;

  // Timers for individual decoding calls indexed by |task_id|.
  std::map<int32_t, base::ElapsedTimer> timers_;
  // Recorded performance metrics.
  std::vector<base::TimeDelta> decode_times_;
  std::vector<base::TimeDelta> decode_map_times_;

  base::WeakPtrFactory<JpegClient> weak_factory_{this};
};

JpegClient::JpegClient(
    const std::vector<DecodeTask>& tasks,
    std::unique_ptr<media::test::ClientStateNotification<ClientState>> note,
    bool use_dmabuf,
    bool skip_result_checking)
    : tasks_(tasks),
      state_(CS_CREATED),
      note_(std::move(note)),
      use_dmabuf_(use_dmabuf),
      skip_result_checking_(skip_result_checking) {}

JpegClient::~JpegClient() = default;

void JpegClient::CreateJpegDecoder() {
  decoder_ = nullptr;

  auto jda_factories =
      GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories();
  if (jda_factories.empty()) {
    LOG(ERROR) << "MjpegDecodeAccelerator not supported on this platform.";
    SetState(CS_ERROR);
    return;
  }

  for (auto& create_jda_func : jda_factories) {
    decoder_ = std::move(create_jda_func)
                   .Run(base::SingleThreadTaskRunner::GetCurrentDefault());
    if (decoder_)
      break;
  }
  if (!decoder_) {
    LOG(ERROR) << "Failed to create MjpegDecodeAccelerator.";
    SetState(CS_ERROR);
    return;
  }

  decoder_->InitializeAsync(this, base::BindOnce(&JpegClient::OnInitialize,
                                                 weak_factory_.GetWeakPtr()));
}

void JpegClient::OnInitialize(bool initialize_result) {
  if (initialize_result) {
    SetState(CS_INITIALIZED);
    return;
  }

  LOG(ERROR) << "MjpegDecodeAccelerator::InitializeAsync() failed";
  SetState(CS_ERROR);
}

void JpegClient::VideoFrameReady(int32_t task_id) {
  const auto timer_iter = timers_.find(task_id);
  ASSERT_TRUE(timer_iter != timers_.end());
  base::ElapsedTimer& timer = timer_iter->second;
  decode_times_.push_back(timer.Elapsed());

  scoped_refptr<media::VideoFrame> mapped_dmabuf_frame;
  if (use_dmabuf_) {
    // Map and convert the output frame to I420.
    mapped_dmabuf_frame = g_env->MapToVideoFrame(
        hw_out_gmb_.get(), hw_out_dmabuf_frame_->layout(),
        hw_out_dmabuf_frame_->visible_rect());
    ASSERT_TRUE(mapped_dmabuf_frame);
    decode_map_times_.push_back(timer.Elapsed());
  }

  timers_.erase(timer_iter);

  if (skip_result_checking_) {
    SetState(CS_DECODE_PASS);
    return;
  }

  if (use_dmabuf_) {
    hw_out_frame_ = media::test::ConvertVideoFrame(mapped_dmabuf_frame.get(),
                                                   media::PIXEL_FORMAT_I420);
    ASSERT_TRUE(hw_out_frame_);
  }

  if (!GetSoftwareDecodeResult(task_id)) {
    SetState(CS_ERROR);
    return;
  }

  if (g_save_to_file) {
    SaveToFile(task_id, hw_out_frame_, "_hw");
    SaveToFile(task_id, sw_out_frame_, "_sw");
  }

  double difference = GetMeanAbsoluteDifference();
  if (difference <= kDecodeSimilarityThreshold) {
    SetState(CS_DECODE_PASS);
  } else {
    LOG(ERROR) << "The mean absolute difference between software and hardware "
               << "decode is " << difference;
    SetState(CS_ERROR);
  }
}

void JpegClient::NotifyError(int32_t task_id,
                             MjpegDecodeAccelerator::Error error) {
  LOG(ERROR) << "Notifying of error " << error << " for task id " << task_id;
  SetState(CS_ERROR);
}

void JpegClient::PrepareMemory(int32_t task_id) {
  const DecodeTask& task = (*tasks_)[task_id];

  if (use_dmabuf_) {
    in_dmabuf_fd_ = g_env->CreateDmaBufFd(task.image->data_str.data(),
                                          task.image->data_str.size());
    ASSERT_TRUE(in_dmabuf_fd_.is_valid());

    // TODO(kamesan): create test cases for more formats when they're used.
    std::vector<media::VideoPixelFormat> supported_formats =
        g_env->GetSupportedDmaBufFormats();
    ASSERT_FALSE(supported_formats.empty());
    hw_out_dmabuf_frame_ = g_env->CreateDmaBufVideoFrame(
        supported_formats[0], task.target_size, task.target_size, &hw_out_gmb_);
    ASSERT_TRUE(hw_out_dmabuf_frame_);
    ASSERT_TRUE(hw_out_gmb_);
  } else {
    in_shm_mapping_ = base::WritableSharedMemoryMapping();
    in_shm_ =
        base::UnsafeSharedMemoryRegion::Create(task.image->data_str.size());
    ASSERT_TRUE(in_shm_.IsValid());
    in_shm_mapping_ = in_shm_.Map();
    ASSERT_TRUE(in_shm_mapping_.IsValid());
    memcpy(in_shm_mapping_.memory(), task.image->data_str.data(),
           task.image->data_str.size());

    // Only I420 output buffer is used in the shared memory path.
    hw_out_frame_ = g_env->CreateMemoryVideoFrame(
        media::PIXEL_FORMAT_I420, task.target_size, task.target_size);
    ASSERT_TRUE(hw_out_frame_);
  }

  if (task.image->visible_size != task.target_size) {
    // Needs an intermediate buffer for cropping/scaling.
    sw_tmp_frame_ = g_env->CreateMemoryVideoFrame(media::PIXEL_FORMAT_I420,
                                                  task.image->coded_size,
                                                  task.image->visible_size);
    ASSERT_TRUE(sw_tmp_frame_);
  }
  sw_out_frame_ = g_env->CreateMemoryVideoFrame(
      media::PIXEL_FORMAT_I420, task.target_size, task.target_size);
  ASSERT_TRUE(sw_out_frame_);
}

void JpegClient::SetState(ClientState new_state) {
  DVLOG(2) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void JpegClient::SaveToFile(int32_t task_id,
                            scoped_refptr<media::VideoFrame> in_frame,
                            const std::string& suffix) {
  LOG_ASSERT(in_frame);
  const DecodeTask& task = (*tasks_)[task_id];

  // First convert to ARGB format. Note that in our case, the coded size and the
  // visible size will be the same.
  scoped_refptr<media::VideoFrame> argb_out_frame =
      media::VideoFrame::CreateFrame(
          media::VideoPixelFormat::PIXEL_FORMAT_ARGB, task.target_size,
          gfx::Rect(task.target_size), task.target_size, base::TimeDelta());
  LOG_ASSERT(argb_out_frame);
  LOG_ASSERT(in_frame->visible_rect() == argb_out_frame->visible_rect());

  // Note that we use J420ToARGB instead of I420ToARGB so that the
  // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
  const int conversion_status = libyuv::J420ToARGB(
      in_frame->visible_data(media::VideoFrame::Plane::kY),
      in_frame->stride(media::VideoFrame::Plane::kY),
      in_frame->visible_data(media::VideoFrame::Plane::kU),
      in_frame->stride(media::VideoFrame::Plane::kU),
      in_frame->visible_data(media::VideoFrame::Plane::kV),
      in_frame->stride(media::VideoFrame::Plane::kV),
      argb_out_frame->GetWritableVisibleData(media::VideoFrame::Plane::kARGB),
      argb_out_frame->stride(media::VideoFrame::Plane::kARGB),
      argb_out_frame->visible_rect().width(),
      argb_out_frame->visible_rect().height());
  LOG_ASSERT(conversion_status == 0);

  // Save as a PNG.
  std::vector<uint8_t> png_output;
  const bool png_encode_status = gfx::PNGCodec::Encode(
      argb_out_frame->visible_data(media::VideoFrame::Plane::kARGB),
      gfx::PNGCodec::FORMAT_BGRA, argb_out_frame->visible_rect().size(),
      argb_out_frame->stride(media::VideoFrame::Plane::kARGB),
      true, /* discard_transparency */
      std::vector<gfx::PNGCodec::Comment>(), &png_output);
  LOG_ASSERT(png_encode_status);
  const base::FilePath in_filename(task.image->filename());
  const base::FilePath out_filename =
      in_filename.ReplaceExtension(".png").InsertBeforeExtension(suffix);
  const bool success = base::WriteFile(out_filename, png_output);
  LOG_ASSERT(success);
}

double JpegClient::GetMeanAbsoluteDifference() {
  double mean_abs_difference = 0;
  size_t num_samples = 0;
  const size_t planes[] = {media::VideoFrame::Plane::kY,
                           media::VideoFrame::Plane::kU,
                           media::VideoFrame::Plane::kV};
  for (size_t plane : planes) {
    const uint8_t* hw_data = hw_out_frame_->data(plane);
    const uint8_t* sw_data = sw_out_frame_->data(plane);
    LOG_ASSERT(hw_out_frame_->visible_rect() == sw_out_frame_->visible_rect());
    const size_t rows =
        media::VideoFrame::Rows(plane, media::PIXEL_FORMAT_I420,
                                hw_out_frame_->visible_rect().height());
    const size_t columns = media::VideoFrame::Columns(
        plane, media::PIXEL_FORMAT_I420, hw_out_frame_->visible_rect().width());
    const int hw_stride = hw_out_frame_->stride(plane);
    const int sw_stride = sw_out_frame_->stride(plane);
    for (size_t row = 0; row < rows; ++row) {
      for (size_t col = 0; col < columns; ++col)
        mean_abs_difference += std::abs(hw_data[col] - sw_data[col]);
      hw_data += hw_stride;
      sw_data += sw_stride;
    }
    num_samples += rows * columns;
  }
  LOG_ASSERT(num_samples > 0);
  mean_abs_difference /= num_samples;
  return mean_abs_difference;
}

void JpegClient::StartDecode(int32_t task_id, bool do_prepare_memory) {
  ASSERT_LT(base::checked_cast<size_t>(task_id), tasks_->size());
  const DecodeTask& task = (*tasks_)[task_id];

  if (do_prepare_memory)
    PrepareMemory(task_id);

  timers_[task_id] = base::ElapsedTimer();
  if (use_dmabuf_) {
    base::ScopedFD duped_in_dmabuf_fd(HANDLE_EINTR(dup(in_dmabuf_fd_.get())));
    ASSERT_TRUE(duped_in_dmabuf_fd.is_valid());
    decoder_->Decode(task_id, std::move(duped_in_dmabuf_fd),
                     task.image->data_str.size(), 0 /* src_offset */,
                     hw_out_dmabuf_frame_);
  } else {
    ASSERT_EQ(in_shm_.GetSize(), task.image->data_str.size());
    media::BitstreamBuffer bitstream_buffer(task_id, in_shm_.Duplicate(),
                                            task.image->data_str.size());
    decoder_->Decode(std::move(bitstream_buffer), hw_out_frame_);
  }
}

bool JpegClient::GetSoftwareDecodeResult(int32_t task_id) {
  const DecodeTask& task = (*tasks_)[task_id];
  const bool do_crop_scale = task.target_size != task.image->visible_size;
  DCHECK(sw_out_frame_->IsMappable());
  DCHECK_EQ(sw_out_frame_->format(), media::PIXEL_FORMAT_I420);
  if (do_crop_scale) {
    DCHECK(sw_tmp_frame_->IsMappable());
    DCHECK_EQ(sw_tmp_frame_->format(), media::PIXEL_FORMAT_I420);
  }
  media::VideoFrame* decode_frame =
      do_crop_scale ? sw_tmp_frame_.get() : sw_out_frame_.get();
  if (libyuv::ConvertToI420(
          reinterpret_cast<const uint8_t*>(task.image->data_str.data()),
          task.image->data_str.size(),
          decode_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
          decode_frame->stride(media::VideoFrame::Plane::kY),
          decode_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
          decode_frame->stride(media::VideoFrame::Plane::kU),
          decode_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
          decode_frame->stride(media::VideoFrame::Plane::kV), 0, 0,
          decode_frame->visible_rect().width(),
          decode_frame->visible_rect().height(),
          decode_frame->visible_rect().width(),
          decode_frame->visible_rect().height(), libyuv::kRotate0,
          libyuv::FOURCC_MJPG) != 0) {
    LOG(ERROR) << "Software decode " << task.image->filename() << " failed.";
    return false;
  }
  if (do_crop_scale) {
    const gfx::Rect crop = media::CropSizeForScalingToTarget(
        sw_tmp_frame_->visible_rect().size(),
        sw_out_frame_->visible_rect().size(), /*alignment=*/2u);
    if (crop.IsEmpty()) {
      LOG(ERROR) << "Failed to calculate crop rectangle for "
                 << sw_tmp_frame_->visible_rect().size().ToString() << " to "
                 << sw_out_frame_->visible_rect().size().ToString();
      return false;
    }
    if (libyuv::I420Scale(
            sw_tmp_frame_->visible_data(media::VideoFrame::Plane::kY) +
                crop.y() * sw_tmp_frame_->stride(media::VideoFrame::Plane::kY) +
                crop.x(),
            sw_tmp_frame_->stride(media::VideoFrame::Plane::kY),
            sw_tmp_frame_->visible_data(media::VideoFrame::Plane::kU) +
                crop.y() / 2 *
                    sw_tmp_frame_->stride(media::VideoFrame::Plane::kU) +
                crop.x() / 2,
            sw_tmp_frame_->stride(media::VideoFrame::Plane::kU),
            sw_tmp_frame_->visible_data(media::VideoFrame::Plane::kV) +
                crop.y() / 2 *
                    sw_tmp_frame_->stride(media::VideoFrame::Plane::kV) +
                crop.x() / 2,
            sw_tmp_frame_->stride(media::VideoFrame::Plane::kV), crop.width(),
            crop.height(),
            sw_out_frame_->GetWritableVisibleData(media::VideoFrame::Plane::kY),
            sw_out_frame_->stride(media::VideoFrame::Plane::kY),
            sw_out_frame_->GetWritableVisibleData(media::VideoFrame::Plane::kU),
            sw_out_frame_->stride(media::VideoFrame::Plane::kU),
            sw_out_frame_->GetWritableVisibleData(media::VideoFrame::Plane::kV),
            sw_out_frame_->stride(media::VideoFrame::Plane::kV),
            sw_out_frame_->visible_rect().width(),
            sw_out_frame_->visible_rect().height(),
            libyuv::kFilterBilinear) != 0) {
      LOG(ERROR) << "Software crop/scale failed.";
      return false;
    }
  }
  return true;
}

PerfMetrics JpegClient::GetPerfMetrics() const {
  return PerfMetrics{
      .num_frames_decoded = decode_times_.size(),
      .total_decode_time = std::accumulate(
          decode_times_.begin(), decode_times_.end(), base::TimeDelta()),
      .total_decode_map_time =
          std::accumulate(decode_map_times_.begin(), decode_map_times_.end(),
                          base::TimeDelta()),
  };
}

// This class holds a |client| that will be deleted on |task_runner|. This is
// necessary because |client->decoder_| expects to be destroyed on the thread on
// which it was created.
class ScopedJpegClient {
 public:
  ScopedJpegClient(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   std::unique_ptr<JpegClient> client)
      : task_runner_(task_runner), client_(std::move(client)) {}

  ScopedJpegClient(const ScopedJpegClient&) = delete;
  ScopedJpegClient& operator=(const ScopedJpegClient&) = delete;

  ~ScopedJpegClient() {
    task_runner_->DeleteSoon(FROM_HERE, std::move(client_));
  }
  JpegClient* client() const { return client_.get(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<JpegClient> client_;
};

class MjpegDecodeAcceleratorTest : public ::testing::TestWithParam<bool> {
 public:
  MjpegDecodeAcceleratorTest(const MjpegDecodeAcceleratorTest&) = delete;
  MjpegDecodeAcceleratorTest& operator=(const MjpegDecodeAcceleratorTest&) =
      delete;

 protected:
  MjpegDecodeAcceleratorTest() = default;

  void TestDecode(const std::vector<DecodeTask>& tasks,
                  const std::vector<ClientState>& expected_status,
                  size_t num_concurrent_decoders = 1);
  void PerfDecodeByJDA(int decode_times, const std::vector<DecodeTask>& tasks);
  void PerfDecodeBySW(int decode_times, const std::vector<DecodeTask>& tasks);

  // This is needed to use base::ThreadPool in MjpegDecodeAccelerator.
  base::test::TaskEnvironment task_environment_;
};

void MjpegDecodeAcceleratorTest::TestDecode(
    const std::vector<DecodeTask>& tasks,
    const std::vector<ClientState>& expected_status,
    size_t num_concurrent_decoders) {
  LOG_ASSERT(tasks.size() >= expected_status.size());
  base::Thread decoder_thread("DecoderThread");
  ASSERT_TRUE(decoder_thread.Start());

  std::vector<std::unique_ptr<ScopedJpegClient>> scoped_clients;

  for (size_t i = 0; i < num_concurrent_decoders; i++) {
    auto client = std::make_unique<JpegClient>(
        tasks,
        std::make_unique<media::test::ClientStateNotification<ClientState>>(),
        GetParam() /* use_dmabuf */, false /* skip_result_checking */);
    scoped_clients.emplace_back(
        new ScopedJpegClient(decoder_thread.task_runner(), std::move(client)));

    decoder_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&JpegClient::CreateJpegDecoder,
                       base::Unretained(scoped_clients.back()->client())));
    ASSERT_EQ(scoped_clients.back()->client()->note()->Wait(), CS_INITIALIZED);
  }

  for (size_t index = 0; index < tasks.size(); index++) {
    for (const auto& scoped_client : scoped_clients) {
      decoder_thread.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&JpegClient::StartDecode,
                                    base::Unretained(scoped_client->client()),
                                    index, true /* do_prepare_memory */));
    }
    if (index < expected_status.size()) {
      for (const auto& scoped_client : scoped_clients) {
        ASSERT_EQ(scoped_client->client()->note()->Wait(),
                  expected_status[index]);
      }
    }
  }
}

void MjpegDecodeAcceleratorTest::PerfDecodeByJDA(
    int decode_times,
    const std::vector<DecodeTask>& tasks) {
  LOG_ASSERT(tasks.size() == 1);
  base::Thread decoder_thread("DecoderThread");
  ASSERT_TRUE(decoder_thread.Start());
  const bool use_dmabuf = GetParam();

  auto client = std::make_unique<JpegClient>(
      tasks,
      std::make_unique<media::test::ClientStateNotification<ClientState>>(),
      use_dmabuf, true /* skip_result_checking */);
  auto scoped_client = std::make_unique<ScopedJpegClient>(
      decoder_thread.task_runner(), std::move(client));

  decoder_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&JpegClient::CreateJpegDecoder,
                                base::Unretained(scoped_client->client())));
  ASSERT_EQ(scoped_client->client()->note()->Wait(), CS_INITIALIZED);

  const int32_t task_id = 0;
  scoped_client->client()->PrepareMemory(task_id);
  for (int index = 0; index < decode_times; index++) {
    decoder_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&JpegClient::StartDecode,
                                  base::Unretained(scoped_client->client()),
                                  task_id, false /* do_prepare_memory */));
    ASSERT_EQ(scoped_client->client()->note()->Wait(), CS_DECODE_PASS);
  }

  const PerfMetrics metrics = scoped_client->client()->GetPerfMetrics();
  const base::TimeDelta avg_decode_time =
      metrics.total_decode_time / metrics.num_frames_decoded;
  LOG(INFO) << "Decode: " << metrics.total_decode_time << " for "
            << metrics.num_frames_decoded
            << " iterations (avg: " << avg_decode_time << ")";
  g_env->AddMetric(
      use_dmabuf ? "hw_jpeg_decode_latency" : "hw_shm_jpeg_decode_latency",
      avg_decode_time);
  if (use_dmabuf) {
    const base::TimeDelta avg_decode_map_time =
        metrics.total_decode_map_time / metrics.num_frames_decoded;
    LOG(INFO) << "Decode + map: " << metrics.total_decode_map_time << " for "
              << metrics.num_frames_decoded
              << " iterations (avg: " << avg_decode_map_time << ")";
    g_env->AddMetric("hw_jpeg_decode_map_latency", avg_decode_map_time);
  }
  LOG(INFO) << "-- " << tasks[0].image->visible_size.ToString() << " ("
            << tasks[0].image->visible_size.GetArea() << " pixels), "
            << tasks[0].image->filename();
}

void MjpegDecodeAcceleratorTest::PerfDecodeBySW(
    int decode_times,
    const std::vector<DecodeTask>& tasks) {
  LOG_ASSERT(tasks.size() == 1);

  std::unique_ptr<JpegClient> client = std::make_unique<JpegClient>(
      tasks,
      std::make_unique<media::test::ClientStateNotification<ClientState>>(),
      false /* use_dmabuf */, true /* skip_result_checking */);

  const int32_t task_id = 0;
  client->PrepareMemory(task_id);
  const base::ElapsedTimer timer;
  for (int index = 0; index < decode_times; index++)
    ASSERT_TRUE(client->GetSoftwareDecodeResult(task_id));
  const base::TimeDelta elapsed_time = timer.Elapsed();
  const base::TimeDelta avg_decode_time = elapsed_time / decode_times;
  LOG(INFO) << "Decode: " << elapsed_time << " for " << decode_times
            << " iterations (avg: " << avg_decode_time << ")";
  LOG(INFO) << "-- " << tasks[0].image->visible_size.ToString() << ", ("
            << tasks[0].image->visible_size.GetArea() << " pixels) "
            << tasks[0].image->filename();
  g_env->AddMetric("sw_jpeg_decode_latency", avg_decode_time);
}

// Returns a media::VideoFrame that contains YUV data using 4:2:0 subsampling.
// The visible size is 3x3, and the coded size is 4x4 which is 3x3 rounded up to
// the next even dimensions.
scoped_refptr<media::VideoFrame> GetTestDecodedData() {
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateZeroInitializedFrame(
          media::PIXEL_FORMAT_I420, gfx::Size(4, 4) /* coded_size */,
          gfx::Rect(3, 3) /* visible_rect */,
          gfx::Size(3, 3) /* natural_size */, base::TimeDelta());
  LOG_ASSERT(frame.get());
  uint8_t* y_data = frame->writable_data(media::VideoFrame::Plane::kY);
  int y_stride = frame->stride(media::VideoFrame::Plane::kY);
  uint8_t* u_data = frame->writable_data(media::VideoFrame::Plane::kU);
  int u_stride = frame->stride(media::VideoFrame::Plane::kU);
  uint8_t* v_data = frame->writable_data(media::VideoFrame::Plane::kV);
  int v_stride = frame->stride(media::VideoFrame::Plane::kV);

  // Data for the Y plane.
  memcpy(&y_data[0 * y_stride], "\x01\x02\x03", 3);
  memcpy(&y_data[1 * y_stride], "\x04\x05\x06", 3);
  memcpy(&y_data[2 * y_stride], "\x07\x08\x09", 3);

  // Data for the U plane.
  memcpy(&u_data[0 * u_stride], "\x0A\x0B", 2);
  memcpy(&u_data[1 * u_stride], "\x0C\x0D", 2);

  // Data for the V plane.
  memcpy(&v_data[0 * v_stride], "\x0E\x0F", 2);
  memcpy(&v_data[1 * v_stride], "\x10\x11", 2);

  return frame;
}

TEST(JpegClientTest, GetMeanAbsoluteDifference) {
  JpegClient client(std::vector<DecodeTask>{}, nullptr, false, false);
  client.hw_out_frame_ = GetTestDecodedData();
  client.sw_out_frame_ = GetTestDecodedData();

  uint8_t* y_data =
      client.sw_out_frame_->writable_data(media::VideoFrame::Plane::kY);
  const int y_stride =
      client.sw_out_frame_->stride(media::VideoFrame::Plane::kY);
  uint8_t* u_data =
      client.sw_out_frame_->writable_data(media::VideoFrame::Plane::kU);
  const int u_stride =
      client.sw_out_frame_->stride(media::VideoFrame::Plane::kU);
  uint8_t* v_data =
      client.sw_out_frame_->writable_data(media::VideoFrame::Plane::kV);
  const int v_stride =
      client.sw_out_frame_->stride(media::VideoFrame::Plane::kV);

  // Change some visible data in the software decoding result.
  double expected_abs_mean_diff = 0;
  y_data[0] = 0xF0;  // Previously 0x01.
  expected_abs_mean_diff += 0xF0 - 0x01;
  y_data[y_stride + 1] = 0x8A;  // Previously 0x05.
  expected_abs_mean_diff += 0x8A - 0x05;
  u_data[u_stride] = 0x02;  // Previously 0x0C.
  expected_abs_mean_diff += 0x0C - 0x02;
  v_data[v_stride + 1] = 0x54;  // Previously 0x11.
  expected_abs_mean_diff += 0x54 - 0x11;
  expected_abs_mean_diff /= 3 * 3 + 2 * 2 * 2;

  constexpr double kMaxAllowedDifference = 1e-7;
  EXPECT_NEAR(expected_abs_mean_diff, client.GetMeanAbsoluteDifference(),
              kMaxAllowedDifference);

  // Change some non-visible data in the software decoding result, i.e., part of
  // the stride padding. This should not affect the absolute mean difference.
  y_data[3] = 0xAB;
  EXPECT_NEAR(expected_abs_mean_diff, client.GetMeanAbsoluteDifference(),
              kMaxAllowedDifference);
}

TEST_P(MjpegDecodeAcceleratorTest, SimpleDecode) {
  std::vector<DecodeTask> tasks;
  for (auto& image : g_env->image_data_user_)
    tasks.emplace_back(image.get());
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status);
}

#if BUILDFLAG(USE_VAAPI)
TEST_P(MjpegDecodeAcceleratorTest, DecodeBlit) {
  std::vector<DecodeTask> tasks;
  for (auto& image : g_env->image_data_user_) {
    tasks.emplace_back(image.get(),
                       gfx::Size((image->visible_size.width() / 2) & ~1,
                                 (image->visible_size.height() / 2) & ~1));
    tasks.emplace_back(image.get(),
                       gfx::Size((image->visible_size.width() / 2) & ~1,
                                 (image->visible_size.height() * 2 / 3) & ~1));
    tasks.emplace_back(image.get(),
                       gfx::Size((image->visible_size.width() * 2 / 3) & ~1,
                                 (image->visible_size.height() / 2) & ~1));
  }
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status);
}
#endif

TEST_P(MjpegDecodeAcceleratorTest, InvalidTargetSize) {
  std::vector<DecodeTask> tasks;
  for (auto& image : g_env->image_data_user_) {
    // Upscaling is not supported.
    tasks.emplace_back(image.get(), gfx::Size(image->visible_size.width() * 2,
                                              image->visible_size.height()));
    tasks.emplace_back(image.get(),
                       gfx::Size(image->visible_size.width(),
                                 image->visible_size.height() * 2));
    // Odd dimensions are not supported.
    tasks.emplace_back(image.get(),
                       gfx::Size((image->visible_size.width() / 2) | 1,
                                 image->visible_size.height() / 2));
    tasks.emplace_back(image.get(),
                       gfx::Size(image->visible_size.width() / 2,
                                 (image->visible_size.height() / 2) | 1));
  }
  const std::vector<ClientState> expected_status(tasks.size(), CS_ERROR);
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, MultipleDecoders) {
  std::vector<DecodeTask> tasks;
  for (auto& image : g_env->image_data_user_)
    tasks.emplace_back(image.get());
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status, 3 /* num_concurrent_decoders */);
}

TEST_P(MjpegDecodeAcceleratorTest, InputSizeChange) {
  // The size of |image_data_1280x720_black_| is smaller than
  // |image_data_1280x720_default_|.
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_1280x720_black_.get()),
      DecodeTask(g_env->image_data_1280x720_default_.get()),
      DecodeTask(g_env->image_data_1280x720_black_.get()),
  };
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, ResolutionChange) {
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_640x368_black_.get()),
      DecodeTask(g_env->image_data_1280x720_default_.get()),
      DecodeTask(g_env->image_data_640x368_black_.get()),
  };
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, CodedSizeAlignment) {
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_640x360_black_.get()),
  };
  const std::vector<ClientState> expected_status = {CS_DECODE_PASS};
  TestDecode(tasks, expected_status);
}

// Tests whether different JPEG sampling formats will be decoded correctly.
TEST_P(MjpegDecodeAcceleratorTest, SamplingFormatChange) {
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_640x368_black_.get()),
      DecodeTask(g_env->image_data_640x368_422_black_.get()),
  };
  const std::vector<ClientState> expected_status(tasks.size(), CS_DECODE_PASS);
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, FailureJpeg) {
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_invalid_.get()),
  };
  const std::vector<ClientState> expected_status = {CS_ERROR};
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, KeepDecodeAfterFailure) {
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_invalid_.get()),
      DecodeTask(g_env->image_data_1280x720_default_.get()),
  };
  const std::vector<ClientState> expected_status = {CS_ERROR, CS_DECODE_PASS};
  TestDecode(tasks, expected_status);
}

TEST_P(MjpegDecodeAcceleratorTest, Abort) {
  constexpr size_t kNumOfJpegToDecode = 5;
  const std::vector<DecodeTask> tasks(
      kNumOfJpegToDecode,
      DecodeTask(g_env->image_data_1280x720_default_.get()));
  // Verify only one decode success to ensure both decoders have started the
  // decoding. Then destroy the first decoder when it is still decoding. The
  // kernel should not crash during this test.
  const std::vector<ClientState> expected_status = {CS_DECODE_PASS};
  TestDecode(tasks, expected_status, 2 /* num_concurrent_decoders */);
}

TEST_P(MjpegDecodeAcceleratorTest, PerfJDA) {
  // Only the first image will be used for perf testing.
  ASSERT_GE(g_env->image_data_user_.size(), 1u);
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_user_[0].get()),
  };
  PerfDecodeByJDA(g_env->perf_decode_times_, tasks);
}

TEST_F(MjpegDecodeAcceleratorTest, PerfSW) {
  // Only the first image will be used for perf testing.
  ASSERT_GE(g_env->image_data_user_.size(), 1u);
  const std::vector<DecodeTask> tasks = {
      DecodeTask(g_env->image_data_user_[0].get()),
  };
  PerfDecodeBySW(g_env->perf_decode_times_, tasks);
}

std::string TestParamToString(::testing::TestParamInfo<bool> param_info) {
  return param_info.param ? "DMABUF" : "SHMEM";
}

INSTANTIATE_TEST_SUITE_P(All,
                         MjpegDecodeAcceleratorTest,
                         ::testing::Bool(),
                         TestParamToString);

}  // namespace
}  // namespace chromeos_camera

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();
  TestTimeouts::Initialize();
  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);

  const base::FilePath::CharType* jpeg_filenames = nullptr;
  const base::FilePath::CharType* test_data_path = nullptr;
  const base::FilePath::CharType* perf_output_path = nullptr;
  int perf_decode_times = 0;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    // jpeg_filenames can include one or many files and use ';' as delimiter.
    if (it->first == "jpeg_filenames") {
      jpeg_filenames = it->second.c_str();
      continue;
    }
    if (it->first == "test_data_path") {
      test_data_path = it->second.c_str();
      continue;
    }
    if (it->first == "perf_output_path") {
      perf_output_path = it->second.c_str();
      continue;
    }
    if (it->first == "perf_decode_times") {
      LOG_ASSERT(base::StringToInt(it->second, &perf_decode_times));
      continue;
    }
    if (it->first == "save_to_file") {
      chromeos_camera::g_save_to_file = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "h" || it->first == "help")
      continue;
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  chromeos_camera::g_env =
      reinterpret_cast<chromeos_camera::MjpegDecodeAcceleratorTestEnvironment*>(
          testing::AddGlobalTestEnvironment(
              new chromeos_camera::MjpegDecodeAcceleratorTestEnvironment(
                  jpeg_filenames, test_data_path, perf_output_path,
                  perf_decode_times)));

  return RUN_ALL_TESTS();
}
