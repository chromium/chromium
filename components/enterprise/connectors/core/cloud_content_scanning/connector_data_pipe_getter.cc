// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/connectors/core/features.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/mman.h>

#include "base/threading/scoped_blocking_call.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/connectors/core/cloud_content_scanning/chunked_file_data_pipe_producer.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace enterprise_connectors {

namespace {
const char kDataContentType[] = "Content-Type: application/octet-stream";

// Write the data from |file_| by chunks of 32 kbs.
constexpr size_t kMaxSize = 32 * 1024;

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ChunkedFileDataPipeProducer> CreateChunkedProducer(
    base::File file,
    bool is_obfuscated) {
  if (!file.IsValid()) {
    return nullptr;
  }

  int64_t file_size = 0;
  std::optional<enterprise_obfuscation::HeaderData> header_data;

  if (is_obfuscated) {
    auto parsed_header =
        enterprise_obfuscation::ObfuscatedFileReader::ReadHeaderData(file);
    if (!parsed_header.has_value()) {
      return nullptr;
    }
    header_data = std::move(parsed_header.value());

    base::File file_clone = file.Duplicate();
    if (!file_clone.IsValid()) {
      return nullptr;
    }
    auto reader = enterprise_obfuscation::ObfuscatedFileReader::Create(
        header_data.value(), std::move(file_clone));
    if (!reader.has_value()) {
      return nullptr;
    }
    file_size = reader->GetSize();
  } else {
    file_size = file.GetLength();
    if (file_size < 0) {
      return nullptr;
    }
  }

  return std::make_unique<ChunkedFileDataPipeProducer>(
      std::move(file), is_obfuscated, file_size, std::move(header_data));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_POSIX)
namespace {
void CloseFileAndMap(base::File file, uint8_t* data, size_t length) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (data) {
    munmap(data, length);
  }
  file.Close();
}
}  // namespace

bool ConnectorDataPipeGetter::InternalMemoryMappedFile::Initialize(
    base::File file) {
  if (IsValid()) {
    return false;
  }

  file_ = std::move(file);

  if (!DoInitialize()) {
    CloseHandles();
    return false;
  }

  return true;
}

bool ConnectorDataPipeGetter::InternalMemoryMappedFile::DoInitialize() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t file_len = file_.GetLength();
  if (file_len < 0) {
    DPLOG(ERROR) << "fstat " << file_.GetPlatformFile();
    return false;
  }
  if (!base::IsValueInRangeForNumericType<size_t>(file_len)) {
    return false;
  }
  void* mapped = mmap(nullptr, static_cast<size_t>(file_len), PROT_READ,
                      MAP_SHARED, file_.GetPlatformFile(), 0);
  if (mapped == MAP_FAILED) {
    LOG(ERROR) << "Upload failure: The creation of a memory mapped file with "
                  "MAP_SHARED failed for file "
               << file_.GetPlatformFile();
    // Retry with MAP_PRIVATE mode.
    // Some file systems do not support MAP_SHARED. Here, it is acceptable to
    // use MAP_PRIVATE instead. Note: For MAP_PRIVATE, it is unspecified whether
    // changes to the underlying file are carried through to the mapped region
    // after the mmap call.
    mapped = mmap(nullptr, static_cast<size_t>(file_len), PROT_READ,
                  MAP_PRIVATE, file_.GetPlatformFile(), 0);
  }
  if (mapped == MAP_FAILED) {
    LOG(ERROR) << "Upload failure: The creation of a memory mapped file with "
                  "MAP_PRIVATE failed for file "
               << file_.GetPlatformFile();
    return false;
  }
  length_ = static_cast<size_t>(file_len);
  data_ = static_cast<uint8_t*>(mapped);
  return true;
}

void ConnectorDataPipeGetter::InternalMemoryMappedFile::CloseHandles() {
  CloseFileAndMap(std::move(file_), data_, length_);
  data_ = nullptr;
  length_ = 0;
}

void ConnectorDataPipeGetter::InternalMemoryMappedFile::CloseHandlesAsync() {
  // Bounce the blocking CloseHandles() call to a background thread
  // so it never blocks the UI thread if destroyed unexpectedly.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CloseFileAndMap, std::move(file_), data_, length_));

  // Prevent CloseHandles() from running again synchronously
  data_ = nullptr;
  length_ = 0;
}

ConnectorDataPipeGetter::InternalMemoryMappedFile::~InternalMemoryMappedFile() {
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis) ||
      !IsValid()) {
    return;
  }

  CloseHandlesAsync();
}

#endif  // BUILDFLAG(IS_POSIX)

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateMultipartPipeGetter(const std::string& boundary,
                                                   const std::string& metadata,
                                                   base::File file,
                                                   bool is_obfuscated) {
  if (!file.IsValid()) {
    return nullptr;
  }

  auto mm_file = std::make_unique<InternalMemoryMappedFile>();
  if (!mm_file->Initialize(std::move(file))) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(
      boundary, metadata, std::move(mm_file), is_obfuscated);
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateMultipartPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page) {
  if (!page.IsValid()) {
    return nullptr;
  }

  base::ReadOnlySharedMemoryMapping mapping = page.Map();
  if (!mapping.IsValid()) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(boundary, metadata,
                                                   std::move(mapping));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateResumablePipeGetter(base::File file,
                                                   bool is_obfuscated) {
  if (!file.IsValid()) {
    return nullptr;
  }

  auto mm_file = std::make_unique<InternalMemoryMappedFile>();
  if (!mm_file->Initialize(std::move(file))) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(
      /*boundary*/ std::string(),
      /*metadata*/ std::string(), std::move(mm_file), is_obfuscated);
}

#if BUILDFLAG(IS_CHROMEOS)
// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateFuseboxResumablePipeGetter(base::File file,
                                                          bool is_obfuscated) {
  auto producer = CreateChunkedProducer(std::move(file), is_obfuscated);
  if (!producer) {
    return nullptr;
  }
  return std::make_unique<ConnectorDataPipeGetter>(std::move(producer));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateFuseboxMultipartPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    base::File file,
    bool is_obfuscated) {
  auto producer = CreateChunkedProducer(std::move(file), is_obfuscated);
  if (!producer) {
    return nullptr;
  }
  return std::make_unique<ConnectorDataPipeGetter>(boundary, metadata,
                                                   std::move(producer));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateResumablePipeGetter(
    base::ReadOnlySharedMemoryRegion page) {
  if (!page.IsValid()) {
    return nullptr;
  }

  base::ReadOnlySharedMemoryMapping mapping = page.Map();
  if (!mapping.IsValid()) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(/*boundary*/ std::string(),
                                                   /*metadata*/ std::string(),
                                                   std::move(mapping));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateResumablePipeGetter(
    scoped_refptr<network::ResourceRequestBody> request_body) {
  if (!request_body) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(/*boundary*/ std::string(),
                                                   /*metadata*/ std::string(),
                                                   std::move(request_body));
}

#if BUILDFLAG(IS_CHROMEOS)
ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    std::unique_ptr<ChunkedFileDataPipeProducer> chunked_file_producer)
    : chunked_file_producer_(std::move(chunked_file_producer)) {}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    std::unique_ptr<ChunkedFileDataPipeProducer> chunked_file_producer)
    : chunked_file_producer_(std::move(chunked_file_producer)) {
  PrepareMultipartRequestFormat(boundary, metadata);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    std::unique_ptr<InternalMemoryMappedFile> file,
    bool is_obfuscated)
    : ConnectorDataPipeGetter(boundary,
                              metadata,
                              std::move(file),
                              /*page=*/base::ReadOnlySharedMemoryMapping(),
                              /*request_body=*/nullptr) {
  CHECK(file_->IsValid());

  if (is_obfuscated) {
    deobfuscator_ =
        std::make_unique<enterprise_obfuscation::DownloadObfuscator>();
  }
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    base::ReadOnlySharedMemoryMapping page)
    : ConnectorDataPipeGetter(boundary,
                              metadata,
                              /*file=*/nullptr,
                              std::move(page),
                              /*request_body=*/nullptr) {
  CHECK(page_.IsValid());
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    scoped_refptr<network::ResourceRequestBody> request_body)
    : ConnectorDataPipeGetter(boundary,
                              metadata,
                              /*file=*/nullptr,
                              /*page=*/base::ReadOnlySharedMemoryMapping(),
                              std::move(request_body)) {
  CHECK(request_body_);
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    std::unique_ptr<InternalMemoryMappedFile> file,
    base::ReadOnlySharedMemoryMapping page,
    scoped_refptr<network::ResourceRequestBody> request_body)
    : file_(std::move(file)),
      page_(std::move(page)),
      request_body_(std::move(request_body)) {
  if (!boundary.empty() && !metadata.empty()) {
    PrepareMultipartRequestFormat(boundary, metadata);
  }
}

ConnectorDataPipeGetter::~ConnectorDataPipeGetter() = default;

void ConnectorDataPipeGetter::Read(mojo::ScopedDataPipeProducerHandle pipe,
                                   ReadCallback callback) {
  Reset();

  if (deobfuscator_ && is_mmap_file_data_pipe()) {
    CHECK(file_->IsValid());
    auto overhead =
        deobfuscator_->CalculateDeobfuscationOverhead(file_->bytes());
    if (!overhead.has_value()) {
      std::move(callback).Run(net::ERR_FAILED, 0);
      return;
    }
    // Pass the size of the deobfuscated data to the data pipe producer.
    std::move(callback).Run(net::OK, FullSize() - overhead.value());
  } else {
    std::move(callback).Run(net::OK, FullSize());
  }

  pipe_ = std::move(pipe);
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  watcher_->Watch(
      pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&ConnectorDataPipeGetter::MojoReadyCallback,
                          base::Unretained(this)));

  Write();
}

void ConnectorDataPipeGetter::Clone(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ConnectorDataPipeGetter::Reset() {
  watcher_.reset();
  pipe_.reset();
  write_position_ = 0;
#if BUILDFLAG(IS_CHROMEOS)
  chunked_buffer_.clear();
  if (chunked_file_producer_) {
    chunked_file_producer_->Reset();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  weak_factory_.InvalidateWeakPtrs();
}

std::unique_ptr<ConnectorDataPipeGetter::InternalMemoryMappedFile>
ConnectorDataPipeGetter::ReleaseFile() {
  return std::move(file_);
}

void ConnectorDataPipeGetter::MojoReadyCallback(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  Write();
}

void ConnectorDataPipeGetter::Write() {
  int64_t metadata_end = metadata_.size();
  if (IsWritePositionInRange(0, metadata_end)) {
    if (!WriteMultipartRequestFormat(metadata_, write_position_)) {
      return;
    }
  }

  int64_t data_end = metadata_end;
  if (is_mmap_file_data_pipe()) {
    data_end += file_->length();
#if BUILDFLAG(IS_CHROMEOS)
  } else if (is_chunked_file_data_pipe()) {
    data_end += chunked_file_producer_->file_size();
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else {
    DCHECK(is_page_data_pipe());
    data_end += page_.size();
  }

  if (IsWritePositionInRange(metadata_end, data_end)) {
    if (is_mmap_file_data_pipe() && !WriteMmapFileData()) {
      return;
    }
    if (is_page_data_pipe() && !WritePageData()) {
      return;
    }
#if BUILDFLAG(IS_CHROMEOS)
    if (is_chunked_file_data_pipe() && !WriteChunkedFileData()) {
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  int64_t last_boundary_end = data_end + last_boundary_.size();
  DCHECK_EQ(last_boundary_end, FullSize());
  if (IsWritePositionInRange(data_end, last_boundary_end)) {
    int64_t offset = write_position_ - data_end;
    if (!WriteMultipartRequestFormat(last_boundary_, offset)) {
      return;
    }
  }

  if (write_position_ == FullSize()) {
    Reset();
  }
}

#if BUILDFLAG(IS_CHROMEOS)
bool ConnectorDataPipeGetter::WriteChunkedFileData() {
  DCHECK(is_chunked_file_data_pipe());

  int64_t file_start = metadata_.size();
  int64_t file_offset = write_position_ - file_start;
  int64_t chunk_offset = file_offset % ChunkedFileDataPipeProducer::kChunkSize;

  if (!chunked_buffer_.empty() &&
      chunk_offset < static_cast<int64_t>(chunked_buffer_.size())) {
    base::span<const uint8_t> bytes = base::span(chunked_buffer_);
    bytes = bytes.subspan(base::checked_cast<size_t>(chunk_offset));

    if (!Write(bytes)) {
      return false;
    }

    chunked_buffer_.clear();
    file_offset = write_position_ - file_start;
  }

  if (chunked_file_producer_->file_fully_read()) {
    return true;
  }

  if (!chunked_file_producer_->is_reading()) {
    chunked_buffer_.clear();
    chunked_file_producer_->ReadNextChunk(
        file_offset, base::BindOnce(&ConnectorDataPipeGetter::OnChunkRead,
                                    weak_factory_.GetWeakPtr()));
  }

  return false;
}

void ConnectorDataPipeGetter::OnChunkRead(std::vector<uint8_t> chunk,
                                          MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    Reset();
    return;
  }

  if (!chunk.empty()) {
    chunked_buffer_ = std::move(chunk);
  }

  Write();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

inline void ConnectorDataPipeGetter::PrepareMultipartRequestFormat(
    const std::string& boundary,
    const std::string& metadata) {
  metadata_ = base::StrCat({"--", boundary, "\r\n", kDataContentType,
                            "\r\n\r\n", metadata, "\r\n--", boundary, "\r\n",
                            kDataContentType, "\r\n\r\n"});

  last_boundary_ = base::StrCat({"\r\n--", boundary, "--\r\n"});
}

bool ConnectorDataPipeGetter::WriteMultipartRequestFormat(
    const std::string& str,
    int64_t offset) {
  CHECK_GE(offset, 0);
  CHECK_LT(offset, static_cast<int64_t>(str.size()));

  base::span<const uint8_t> bytes = base::as_byte_span(str);
  bytes = bytes.subspan(base::checked_cast<size_t>(offset));
  return Write(bytes);
}

bool ConnectorDataPipeGetter::WriteMmapFileData() {
  int64_t file_offset = write_position_ - metadata_.size();
  CHECK(file_->IsValid());
  CHECK_GE(file_offset, 0);
  CHECK_LT(file_offset, static_cast<int64_t>(file_->length()));

  base::span<const uint8_t> bytes = file_->bytes();
  bytes = bytes.subspan(base::checked_cast<size_t>(file_offset));

  if (!deobfuscator_) {
    return Write(bytes);
  }

  // For obfuscated files, we deobfuscate chunk by chunk and write it
  // incrementally.
  while (!bytes.empty()) {
    auto deobfuscated_chunk = deobfuscator_->GetNextDeobfuscatedChunk(bytes);
    if (!deobfuscated_chunk.has_value()) {
      Reset();
      return false;
    }

    if (!Write(deobfuscated_chunk.value())) {
      return false;
    }

    size_t offset = deobfuscator_->GetNextChunkOffset();

    // Update positions and move to the next chunk to deobfuscate.
    write_position_ += offset;
    bytes = bytes.subspan(offset);
  }
  return true;
}

bool ConnectorDataPipeGetter::WritePageData() {
  int64_t page_offset = write_position_ - metadata_.size();
  CHECK_GE(page_offset, 0);
  CHECK_LT(page_offset, static_cast<int64_t>(page_.size()));

  base::span<const uint8_t> bytes = page_.GetMemoryAsSpan<uint8_t>();
  bytes = bytes.subspan(base::checked_cast<size_t>(page_offset));
  return Write(bytes);
}

bool ConnectorDataPipeGetter::Write(base::span<const uint8_t> data) {
  while (true) {
    if (data.empty()) {
      // The data is fully read, so allow the next Write.
      return true;
    }

    size_t actually_written_bytes = 0;
    int result =
        pipe_->WriteData(data.first(std::min(kMaxSize, data.size())),
                         MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      watcher_->ArmOrNotify();
      return false;
    } else if (result != MOJO_RESULT_OK) {
      Reset();
      return false;
    }

    data = data.subspan(actually_written_bytes);
    if (deobfuscator_) {
      // Update write position within the current deobfuscated chunk for the
      // next call.
      deobfuscator_->UpdateDeobfuscatedChunkPosition(actually_written_bytes);
    } else {
      write_position_ += actually_written_bytes;
    }
  }
}

bool ConnectorDataPipeGetter::IsWritePositionInRange(int64_t range_start,
                                                     int64_t range_end) {
  return (range_start <= write_position_ && write_position_ < range_end);
}

int64_t ConnectorDataPipeGetter::FullSize() {
  int64_t size = metadata_.size() + last_boundary_.size();
#if BUILDFLAG(IS_CHROMEOS)
  if (is_chunked_file_data_pipe()) {
    return size + chunked_file_producer_->file_size();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (is_mmap_file_data_pipe()) {
    return size + file_->length();
  } else {
    DCHECK(is_page_data_pipe());
    return size + page_.size();
  }
}

bool ConnectorDataPipeGetter::is_mmap_file_data_pipe() const {
  return file_.get() != nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
bool ConnectorDataPipeGetter::is_chunked_file_data_pipe() const {
  return chunked_file_producer_.get() != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool ConnectorDataPipeGetter::is_page_data_pipe() const {
  return page_.data();
}

bool ConnectorDataPipeGetter::is_network_request_data_pipe() const {
  return request_body_.get();
}

}  // namespace enterprise_connectors
