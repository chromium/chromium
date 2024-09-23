// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/udif.h"

#include <CoreFoundation/CoreFoundation.h>
#include <bzlib.h>
#include <libkern/OSByteOrder.h>
#include <uuid/uuid.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/ostream_operators.h"
#include "base/numerics/safe_math.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/utility/safe_browsing/mac/convert_big_endian.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "third_party/zlib/zlib.h"

namespace safe_browsing {
namespace dmg {

#pragma pack(push, 1)

// The following structures come from the analysis provided by Jonathan Levin
// at <http://newosxbook.com/DMG.html>.
//
// Note that all fields are stored in big endian.

struct UDIFChecksum {
  uint32_t type;
  uint32_t size;
  std::array<uint32_t, 32> data;
};

static void ConvertBigEndian(UDIFChecksum* checksum) {
  ConvertBigEndian(&checksum->type);
  ConvertBigEndian(&checksum->size);
  for (size_t i = 0; i < std::size(checksum->data); ++i) {
    ConvertBigEndian(&checksum->data[i]);
  }
}

// The trailer structure for a UDIF file.
struct UDIFResourceFile {
  static const uint32_t kSignature = 'koly';
  static const uint32_t kVersion = 4;

  uint32_t signature;
  uint32_t version;
  uint32_t header_size;  // Size of this structure.
  uint32_t flags;
  uint64_t running_data_fork_offset;
  uint64_t data_fork_offset;
  uint64_t data_fork_length;
  uint64_t rsrc_fork_offset;
  uint64_t rsrc_fork_length;
  uint32_t segment_number;
  uint32_t segment_count;
  uuid_t   segment_id;

  UDIFChecksum data_checksum;

  uint64_t plist_offset;  // Offset and length of the blkx plist.
  uint64_t plist_length;

  uint8_t  reserved1[64];

  uint64_t code_signature_offset;
  uint64_t code_signature_length;

  uint8_t  reserved2[40];

  UDIFChecksum main_checksum;

  uint32_t image_variant;
  uint64_t sector_count;

  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t reserved5;
};

static void ConvertBigEndian(uuid_t* uuid) {
  // UUID is never consulted, so do not swap.
}

static void ConvertBigEndian(UDIFResourceFile* file) {
  ConvertBigEndian(&file->signature);
  ConvertBigEndian(&file->version);
  ConvertBigEndian(&file->flags);
  ConvertBigEndian(&file->header_size);
  ConvertBigEndian(&file->running_data_fork_offset);
  ConvertBigEndian(&file->data_fork_offset);
  ConvertBigEndian(&file->data_fork_length);
  ConvertBigEndian(&file->rsrc_fork_offset);
  ConvertBigEndian(&file->rsrc_fork_length);
  ConvertBigEndian(&file->segment_number);
  ConvertBigEndian(&file->segment_count);
  ConvertBigEndian(&file->segment_id);
  ConvertBigEndian(&file->data_checksum);
  ConvertBigEndian(&file->plist_offset);
  ConvertBigEndian(&file->plist_length);
  ConvertBigEndian(&file->code_signature_offset);
  ConvertBigEndian(&file->code_signature_length);
  ConvertBigEndian(&file->main_checksum);
  ConvertBigEndian(&file->image_variant);
  ConvertBigEndian(&file->sector_count);
  // Reserved fields are skipped.
}

struct UDIFBlockChunk {
  enum class Type : uint32_t {
    ZERO_FILL     = 0x00000000,
    UNCOMPRESSED  = 0x00000001,
    IGNORED       = 0x00000002,
    COMPRESS_ADC  = 0x80000004,
    COMPRESS_ZLIB = 0x80000005,
    COMPRESSS_BZ2 = 0x80000006,
    COMMENT       = 0x7ffffffe,
    LAST_BLOCK    = 0xffffffff,
  };

  Type type;
  uint32_t comment;
  uint64_t start_sector;  // Logical chunk offset and length, in sectors.
  uint64_t sector_count;
  uint64_t compressed_offset;  // Compressed offset and length, in bytes.
  uint64_t compressed_length;
};

static void ConvertBigEndian(UDIFBlockChunk* chunk) {
  ConvertBigEndian(reinterpret_cast<uint32_t*>(&chunk->type));
  ConvertBigEndian(&chunk->comment);
  ConvertBigEndian(&chunk->start_sector);
  ConvertBigEndian(&chunk->sector_count);
  ConvertBigEndian(&chunk->compressed_offset);
  ConvertBigEndian(&chunk->compressed_length);
}

struct UDIFBlockData {
  static const uint32_t kSignature = 'mish';
  static const uint32_t kVersion = 1;

  uint32_t signature;
  uint32_t version;
  uint64_t start_sector;  // Logical block offset and length, in sectors.
  uint64_t sector_count;

  uint64_t data_offset;
  uint32_t buffers_needed;
  uint32_t block_descriptors;

  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t reserved5;
  uint32_t reserved6;

  UDIFChecksum checksum;

  uint32_t chunk_count;
};

static void ConvertBigEndian(UDIFBlockData* block) {
  ConvertBigEndian(&block->signature);
  ConvertBigEndian(&block->version);
  ConvertBigEndian(&block->start_sector);
  ConvertBigEndian(&block->sector_count);
  ConvertBigEndian(&block->data_offset);
  ConvertBigEndian(&block->buffers_needed);
  ConvertBigEndian(&block->block_descriptors);
  // Reserved fields are skipped.
  ConvertBigEndian(&block->checksum);
  ConvertBigEndian(&block->chunk_count);
}

// UDIFBlock takes a raw, big-endian block data pointer and stores, in host
// endian, the data for both the block and the chunk.
class UDIFBlock {
 public:
  UDIFBlock() : block_() {}

  UDIFBlock(const UDIFBlock&) = delete;
  UDIFBlock& operator=(const UDIFBlock&) = delete;

  bool ParseBlockData(base::span<const uint8_t> block_data,
                      uint16_t sector_size) {
    base::BufferIterator iterator(block_data);
    const UDIFBlockData* block_header = iterator.Object<UDIFBlockData>();
    if (!block_header) {
      DLOG(ERROR) << "UDIF block data is smaller than expected";
      return false;
    }

    block_ = *block_header;
    ConvertBigEndian(&block_);

    // Make sure the number of sectors doesn't overflow.
    auto block_size = base::CheckedNumeric<size_t>(sector_count()) *
                      sector_size;
    if (!block_size.IsValid()) {
      DLOG(ERROR) << "UDIF block size overflows";
      return false;
    }

    // Make sure the block data contains the reported number of chunks.
    auto block_and_chunks_size =
        (base::CheckedNumeric<size_t>(sizeof(UDIFBlockChunk)) *
         block_.chunk_count) +
        sizeof(block_);
    if (!block_and_chunks_size.IsValid() ||
        block_data.size() < block_and_chunks_size.ValueOrDie()) {
      DLOG(ERROR) << "UDIF block does not contain reported number of chunks, "
                  << block_and_chunks_size.ValueOrDie() << " bytes expected, "
                  << "got " << block_data.size();
      return false;
    }

    // Make sure that the chunk data isn't larger than the block reports.
    base::CheckedNumeric<size_t> chunk_sectors(0);
    for (uint32_t i = 0; i < block_.chunk_count; ++i) {
      const UDIFBlockChunk* raw_chunk = iterator.Object<UDIFBlockChunk>();
      // Total size check above should ensure that the chunk always exists
      CHECK(raw_chunk);
      chunks_.push_back(*raw_chunk);

      UDIFBlockChunk* chunk = &chunks_[i];
      ConvertBigEndian(chunk);

      chunk_sectors += chunk->sector_count;
      if (!chunk_sectors.IsValid() ||
          chunk_sectors.ValueOrDie() > sector_count()) {
        DLOG(ERROR) << "Total chunk sectors larger than reported block sectors";
        return false;
      }

      auto chunk_end_offset =
          base::CheckedNumeric<size_t>(chunk->compressed_offset) +
          chunk->compressed_length;
      if (!chunk_end_offset.IsValid() ||
          chunk->compressed_length > block_size.ValueOrDie()) {
        DLOG(ERROR) << "UDIF chunk data length " << i << " overflows";
        return false;
      }
    }

    return true;
  }

  uint32_t signature() const { return block_.signature; }
  uint32_t version() const { return block_.version; }
  uint64_t start_sector() const { return block_.start_sector; }
  uint64_t sector_count() const { return block_.sector_count; }
  uint64_t chunk_count() const { return chunks_.size(); }

  const UDIFBlockChunk* chunk(uint32_t i) const {
    if (i >= chunk_count())
      return nullptr;
    return &chunks_[i];
  }

 private:
  UDIFBlockData block_;
  std::vector<UDIFBlockChunk> chunks_;
};

#pragma pack(pop)

namespace {

const size_t kSectorSize = 512;

class UDIFBlockChunkReadStream;

// A UDIFPartitionReadStream virtualizes a partition's non-contiguous blocks
// into a single stream.
class UDIFPartitionReadStream : public ReadStream {
 public:
  UDIFPartitionReadStream(ReadStream* stream,
                          uint16_t block_size,
                          const UDIFBlock* partition_block);

  UDIFPartitionReadStream(const UDIFPartitionReadStream&) = delete;
  UDIFPartitionReadStream& operator=(const UDIFPartitionReadStream&) = delete;

  ~UDIFPartitionReadStream() override;

  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override;
  // Seek only supports SEEK_SET and SEEK_CUR.
  off_t Seek(off_t offset, int whence) override;

 private:
  const raw_ptr<ReadStream> stream_;  // The UDIF stream.
  const uint16_t block_size_;  // The UDIF block size.
  const raw_ptr<const UDIFBlock> block_;  // The block for this partition.
  uint64_t current_chunk_;  // The current chunk number.
  // The current chunk stream.
  std::unique_ptr<UDIFBlockChunkReadStream> chunk_stream_;
};

// A ReadStream for a single block chunk, which transparently handles
// decompression.
class UDIFBlockChunkReadStream : public ReadStream {
 public:
  UDIFBlockChunkReadStream(ReadStream* stream,
                           uint16_t block_size,
                           const UDIFBlockChunk* chunk);

  UDIFBlockChunkReadStream(const UDIFBlockChunkReadStream&) = delete;
  UDIFBlockChunkReadStream& operator=(const UDIFBlockChunkReadStream&) = delete;

  ~UDIFBlockChunkReadStream() override;

  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override;
  // Seek only supports SEEK_SET.
  off_t Seek(off_t offset, int whence) override;

  bool IsAtEnd() { return offset_ >= length_in_bytes_; }

  const UDIFBlockChunk* chunk() const { return chunk_; }
  size_t length_in_bytes() const { return length_in_bytes_; }

 private:
  bool CopyOutZeros(base::span<uint8_t> buf, size_t* bytes_read);
  bool CopyOutUncompressed(base::span<uint8_t> buf, size_t* bytes_read);
  bool CopyOutDecompressed(base::span<uint8_t> buf, size_t* bytes_read);
  bool HandleADC(base::span<uint8_t> buf, size_t* bytes_read);
  bool HandleZLib(base::span<uint8_t> buf, size_t* bytes_read);
  bool HandleBZ2(base::span<uint8_t> buf, size_t* bytes_read);

  // Reads from |stream_| |chunk_->compressed_length| bytes, starting at
  // |chunk_->compressed_offset|. Returns (possibly empty) vector containing
  // data, or nullopt on error.
  std::optional<std::vector<uint8_t>> ReadCompressedData();

  const raw_ptr<ReadStream> stream_;           // The UDIF stream.
  const raw_ptr<const UDIFBlockChunk> chunk_;  // The chunk to be read.
  size_t length_in_bytes_;  // The decompressed length in bytes.
  size_t offset_;  // The offset into the decompressed buffer.
  std::vector<uint8_t> decompress_buffer_;  // Decompressed data buffer.
  bool did_decompress_;  // Whether or not the chunk has been decompressed.
};

}  // namespace

UDIFParser::UDIFParser(ReadStream* stream)
    : stream_(stream),
      partition_names_(),
      blocks_(),
      block_size_(kSectorSize) {}

UDIFParser::~UDIFParser() {}

bool UDIFParser::Parse() {
  if (!ParseBlkx())
    return false;

  return true;
}

const std::vector<uint8_t>& UDIFParser::GetCodeSignature() {
  return signature_blob_;
}

size_t UDIFParser::GetNumberOfPartitions() {
  return blocks_.size();
}

std::string UDIFParser::GetPartitionName(size_t part_number) {
  DCHECK_LT(part_number, partition_names_.size());
  return partition_names_[part_number];
}

std::string UDIFParser::GetPartitionType(size_t part_number) {
  // The partition type is embedded in the Name field, as such:
  // "Partition-Name (Partition-Type : Partition-ID)".
  std::string name = GetPartitionName(part_number);
  size_t open = name.rfind('(');
  size_t separator = name.rfind(':');
  if (open == std::string::npos || separator == std::string::npos)
    return std::string();

  // Name does not end in ')' or no space after ':'.
  if (*(name.end() - 1) != ')' ||
      (name.size() - separator < 2 || name[separator + 1] != ' ')) {
    return std::string();
  }

  --separator;
  ++open;
  if (separator <= open)
    return std::string();
  return name.substr(open, separator - open);
}

size_t UDIFParser::GetPartitionSize(size_t part_number) {
  DCHECK_LT(part_number, blocks_.size());
  auto size =
      base::CheckedNumeric<size_t>(blocks_[part_number]->sector_count()) *
      block_size_;
  return size.ValueOrDie();
}

std::unique_ptr<ReadStream> UDIFParser::GetPartitionReadStream(
    size_t part_number) {
  DCHECK_LT(part_number, blocks_.size());
  return std::make_unique<UDIFPartitionReadStream>(stream_, block_size_,
                                                   blocks_[part_number].get());
}

bool UDIFParser::ParseBlkx() {
  UDIFResourceFile trailer;
  off_t trailer_start = stream_->Seek(-sizeof(trailer), SEEK_END);
  if (trailer_start == -1)
    return false;

  if (!stream_->ReadType(trailer)) {
    DLOG(ERROR) << "Failed to read UDIFResourceFile";
    return false;
  }
  ConvertBigEndian(&trailer);

  if (trailer.signature != trailer.kSignature) {
    DLOG(ERROR) << "blkx signature does not match, is 0x"
                << std::hex << trailer.signature;
    return false;
  }
  if (trailer.version != trailer.kVersion) {
    DLOG(ERROR) << "blkx version does not match, is " << trailer.version;
    return false;
  }

  auto plist_end = base::CheckedNumeric<size_t>(trailer.plist_offset) +
                   trailer.plist_length;
  if (!plist_end.IsValid() ||
      plist_end.ValueOrDie() > base::checked_cast<size_t>(trailer_start)) {
    DLOG(ERROR) << "blkx plist extends past UDIF trailer";
    return false;
  }

  std::vector<uint8_t> plist_bytes(trailer.plist_length, 0);

  if (stream_->Seek(trailer.plist_offset, SEEK_SET) == -1)
    return false;

  if (trailer.plist_length == 0 || !stream_->ReadExact(plist_bytes)) {
    DLOG(ERROR) << "Failed to read blkx plist data";
    return false;
  }

  base::apple::ScopedCFTypeRef<CFDataRef> plist_data(
      CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, plist_bytes.data(),
                                  plist_bytes.size(), kCFAllocatorNull));
  if (!plist_data) {
    DLOG(ERROR) << "Failed to create data from bytes";
    return false;
  }

  CFErrorRef error = nullptr;
  base::apple::ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPropertyListCreateWithData(kCFAllocatorDefault, plist_data.get(),
                                   kCFPropertyListImmutable, nullptr, &error));

  CFDictionaryRef plist_dict =
      base::apple::CFCast<CFDictionaryRef>(plist.get());
  base::apple::ScopedCFTypeRef<CFErrorRef> error_ref(error);
  if (error) {
    base::apple::ScopedCFTypeRef<CFStringRef> error_string(
        CFErrorCopyDescription(error));
    DLOG(ERROR) << "Failed to parse XML plist: "
                << base::SysCFStringRefToUTF8(error_string.get());
    return false;
  }

  if (!plist_dict) {
    DLOG(ERROR) << "Plist is not a dictionary";
    return false;
  }

  auto* resource_fork = base::apple::GetValueFromDictionary<CFDictionaryRef>(
      plist_dict, CFSTR("resource-fork"));
  if (!resource_fork) {
    DLOG(ERROR) << "No resource-fork entry in plist";
    return false;
  }

  auto* blkx = base::apple::GetValueFromDictionary<CFArrayRef>(resource_fork,
                                                               CFSTR("blkx"));
  if (!blkx) {
    DLOG(ERROR) << "No blkx entry in resource-fork";
    return false;
  }

  for (CFIndex i = 0; i < CFArrayGetCount(blkx); ++i) {
    auto* block_dictionary =
        base::apple::CFCast<CFDictionaryRef>(CFArrayGetValueAtIndex(blkx, i));
    if (!block_dictionary) {
      DLOG(ERROR) << "Skipping block " << i
                  << " because it is not a CFDictionary";
      continue;
    }

    auto* data = base::apple::GetValueFromDictionary<CFDataRef>(
        block_dictionary, CFSTR("Data"));
    if (!data) {
      DLOG(ERROR) << "Skipping block " << i
                  << " because it has no Data section";
      continue;
    }

    // Copy the block table out of the plist.
    auto block = std::make_unique<UDIFBlock>();
    // SAFETY: CFDataGetBytePtr is provided by Apple and documented to
    // return CFDataGetLength bytes.
    if (!block->ParseBlockData(UNSAFE_BUFFERS(
            base::span(CFDataGetBytePtr(data),
                       base::checked_cast<size_t>(CFDataGetLength(data))),
            block_size_))) {
      DLOG(ERROR) << "Failed to parse UDIF block data";
      return false;
    }

    if (block->signature() != UDIFBlockData::kSignature) {
      DLOG(ERROR) << "Skipping block " << i << " because its signature does not"
                  << " match, is 0x" << std::hex << block->signature();
      continue;
    }
    if (block->version() != UDIFBlockData::kVersion) {
      DLOG(ERROR) << "Skipping block " << i << "because its version does not "
                  << "match, is " << block->version();
      continue;
    }

    CFStringRef partition_name_cf = base::apple::CFCast<CFStringRef>(
        CFDictionaryGetValue(block_dictionary, CFSTR("Name")));
    if (!partition_name_cf) {
      DLOG(ERROR) << "Skipping block " << i << " because it has no name";
      continue;
    }
    std::string partition_name = base::SysCFStringRefToUTF8(partition_name_cf);

    if (DLOG_IS_ON(INFO) && VLOG_IS_ON(1)) {
      DVLOG(1) << "Name: " << partition_name;
      DVLOG(1) << "StartSector = " << block->start_sector()
               << ", SectorCount = " << block->sector_count()
               << ", ChunkCount = " << block->chunk_count();
      for (uint32_t j = 0; j < block->chunk_count(); ++j) {
        const UDIFBlockChunk* chunk = block->chunk(j);
        DVLOG(1) << "Chunk#" << j
                 << " type = " << std::hex << static_cast<uint32_t>(chunk->type)
                 << ", StartSector = " << std::dec << chunk->start_sector
                 << ", SectorCount = " << chunk->sector_count
                 << ", CompressOffset = " << chunk->compressed_offset
                 << ", CompressLen = " << chunk->compressed_length;
      }
    }

    blocks_.push_back(std::move(block));
    partition_names_.push_back(partition_name);
  }

  // The offsets in the trailer could be garbage in DMGs that aren't signed.
  // Need a sanity check that the DMG has legit values for these fields.
  if (trailer.code_signature_length != 0 && trailer_start > 0) {
    auto code_signature_end =
        base::CheckedNumeric<size_t>(trailer.code_signature_offset) +
        trailer.code_signature_length;
    if (code_signature_end.IsValid() &&
        code_signature_end.ValueOrDie() <=
            base::checked_cast<size_t>(trailer_start)) {
      signature_blob_.resize(trailer.code_signature_length);

      off_t code_signature_start =
          stream_->Seek(trailer.code_signature_offset, SEEK_SET);
      if (code_signature_start == -1)
        return false;

      size_t bytes_read = 0;

      if (!stream_->Read(signature_blob_, &bytes_read)) {
        DLOG(ERROR) << "Failed to read raw signature bytes";
        return false;
      }

      if (bytes_read != trailer.code_signature_length) {
        DLOG(ERROR) << "Read unexpected number of raw signature bytes";
        return false;
      }
    }
  }

  return true;
}

namespace {

UDIFPartitionReadStream::UDIFPartitionReadStream(
    ReadStream* stream,
    uint16_t block_size,
    const UDIFBlock* partition_block)
    : stream_(stream),
      block_size_(block_size),
      block_(partition_block),
      current_chunk_(0),
      chunk_stream_() {
}

UDIFPartitionReadStream::~UDIFPartitionReadStream() {}

bool UDIFPartitionReadStream::Read(base::span<uint8_t> buf,
                                   size_t* bytes_read) {
  size_t buffer_space_remaining = buf.size();
  *bytes_read = 0;

  for (uint32_t i = current_chunk_; i < block_->chunk_count(); ++i) {
    const UDIFBlockChunk* chunk = block_->chunk(i);

    // If this is the last block chunk, then the read is complete.
    if (chunk->type == UDIFBlockChunk::Type::LAST_BLOCK) {
      break;
    }

    // If the buffer is full, then the read is complete.
    if (buffer_space_remaining == 0)
      break;

    // A chunk stream may exist if the last read from this chunk was partial,
    // or if the stream was Seek()ed.
    if (!chunk_stream_) {
      chunk_stream_ = std::make_unique<UDIFBlockChunkReadStream>(
          stream_, block_size_, chunk);
    }
    DCHECK_EQ(chunk, chunk_stream_->chunk());

    size_t chunk_bytes_read = 0;
    if (!chunk_stream_->Read(buf.last(buffer_space_remaining),
                             &chunk_bytes_read)) {
      DLOG(ERROR) << "Failed to read " << buffer_space_remaining << " bytes "
                  << "from chunk " << i;
      return false;
    }
    *bytes_read += chunk_bytes_read;
    buffer_space_remaining -= chunk_bytes_read;

    if (chunk_stream_->IsAtEnd()) {
      chunk_stream_.reset();
      ++current_chunk_;
    }
  }

  return true;
}

off_t UDIFPartitionReadStream::Seek(off_t offset, int whence) {
  // Translate SEEK_END to SEEK_SET. SEEK_CUR is not currently supported.
  if (whence == SEEK_END) {
    base::CheckedNumeric<off_t> safe_offset(block_->sector_count());
    safe_offset *= block_size_;
    safe_offset += offset;
    if (!safe_offset.IsValid()) {
      DLOG(ERROR) << "Seek offset overflows";
      return -1;
    }
    offset = safe_offset.ValueOrDie();
  } else if (whence != SEEK_SET) {
    DCHECK_EQ(SEEK_SET, whence);
  }

  uint64_t sector = offset / block_size_;

  // Find the chunk for this sector.
  uint32_t chunk_number = 0;
  const UDIFBlockChunk* chunk = nullptr;
  for (uint32_t i = 0; i < block_->chunk_count(); ++i) {
    const UDIFBlockChunk* chunk_it = block_->chunk(i);
    // This assumes that all the chunks are ordered by sector.
    if (i != 0) {
      DLOG_IF(ERROR,
              chunk_it->start_sector < block_->chunk(i - 1)->start_sector)
          << "Chunks are not ordered by sector at chunk " << i
          << " , previous start_sector = "
          << block_->chunk(i - 1)->start_sector << ", current = "
          << chunk_it->start_sector;
    }
    if (sector >= chunk_it->start_sector) {
      chunk = chunk_it;
      chunk_number = i;
    } else {
      break;
    }
  }
  if (!chunk) {
    DLOG(ERROR) << "Failed to Seek to partition offset " << offset;
    return -1;
  }

  // Compute the offset into the chunk.
  uint64_t offset_in_sector = offset % block_size_;
  uint64_t start_sector = sector - chunk->start_sector;
  base::CheckedNumeric<uint64_t> decompress_read_offset(start_sector);
  decompress_read_offset *= block_size_;
  decompress_read_offset += offset_in_sector;

  if (!decompress_read_offset.IsValid()) {
    DLOG(ERROR) << "Partition decompress read offset overflows";
    return -1;
  }

  if (!chunk_stream_ || chunk != chunk_stream_->chunk()) {
    chunk_stream_ =
        std::make_unique<UDIFBlockChunkReadStream>(stream_, block_size_, chunk);
  }
  current_chunk_ = chunk_number;
  if (chunk_stream_->Seek(
          base::ValueOrDieForType<off_t>(decompress_read_offset), SEEK_SET) ==
      -1)
    return -1;

  return offset;
}

UDIFBlockChunkReadStream::UDIFBlockChunkReadStream(ReadStream* stream,
                                                   uint16_t block_size,
                                                   const UDIFBlockChunk* chunk)
    : stream_(stream),
      chunk_(chunk),
      length_in_bytes_(chunk->sector_count * block_size),
      offset_(0),
      decompress_buffer_(),
      did_decompress_(false) {
  // Make sure the multiplication above did not overflow.
  CHECK(length_in_bytes_ == 0 || length_in_bytes_ >= block_size);
}

UDIFBlockChunkReadStream::~UDIFBlockChunkReadStream() {
}

bool UDIFBlockChunkReadStream::Read(base::span<uint8_t> buf,
                                    size_t* bytes_read) {
  switch (chunk_->type) {
    case UDIFBlockChunk::Type::ZERO_FILL:
    case UDIFBlockChunk::Type::IGNORED:
      return CopyOutZeros(buf, bytes_read);
    case UDIFBlockChunk::Type::UNCOMPRESSED:
      return CopyOutUncompressed(buf, bytes_read);
    case UDIFBlockChunk::Type::COMPRESS_ADC:
      return HandleADC(buf, bytes_read);
    case UDIFBlockChunk::Type::COMPRESS_ZLIB:
      return HandleZLib(buf, bytes_read);
    case UDIFBlockChunk::Type::COMPRESSS_BZ2:
      return HandleBZ2(buf, bytes_read);
    case UDIFBlockChunk::Type::COMMENT:
      NOTREACHED_IN_MIGRATION();
      break;
    case UDIFBlockChunk::Type::LAST_BLOCK:
      *bytes_read = 0;
      return true;
  }
  return false;
}

off_t UDIFBlockChunkReadStream::Seek(off_t offset, int whence) {
  DCHECK_EQ(SEEK_SET, whence);
  if (static_cast<uint64_t>(offset) >= length_in_bytes_)
    return -1;
  offset_ = offset;
  return offset_;
}

bool UDIFBlockChunkReadStream::CopyOutZeros(base::span<uint8_t> buf,
                                            size_t* bytes_read) {
  *bytes_read = std::min(buf.size(), length_in_bytes_ - offset_);
  bzero(buf.data(), *bytes_read);
  offset_ += *bytes_read;
  return true;
}

bool UDIFBlockChunkReadStream::CopyOutUncompressed(base::span<uint8_t> buf,
                                                   size_t* bytes_read) {
  *bytes_read = std::min(buf.size(), length_in_bytes_ - offset_);

  if (*bytes_read == 0) {
    return true;
  }

  uint64_t offset = chunk_->compressed_offset + offset_;
  if (stream_->Seek(offset, SEEK_SET) == -1) {
    return false;
  }

  bool rv = stream_->Read(buf.first(*bytes_read), bytes_read);
  if (rv) {
    offset_ += *bytes_read;
  } else {
    DLOG(ERROR) << "Failed to read uncompressed chunk data";
  }
  return rv;
}

bool UDIFBlockChunkReadStream::CopyOutDecompressed(base::span<uint8_t> buf,
                                                   size_t* bytes_read) {
  DCHECK(did_decompress_);
  *bytes_read = std::min(buf.size(), decompress_buffer_.size() - offset_);
  base::span<uint8_t> src_data =
      base::span(decompress_buffer_).subspan(offset_, *bytes_read);
  buf.copy_prefix_from(src_data);
  offset_ += *bytes_read;
  return true;
}

bool UDIFBlockChunkReadStream::HandleADC(base::span<uint8_t> buf,
                                         size_t* bytes_read) {
  // TODO(rsesek): Implement ADC handling.
  NOTIMPLEMENTED();
  return false;
}

bool UDIFBlockChunkReadStream::HandleZLib(base::span<uint8_t> buf,
                                          size_t* bytes_read) {
  if (!did_decompress_) {
    auto compressed_data_or_error = ReadCompressedData();
    if (!compressed_data_or_error.has_value()) {
      return false;
    }
    std::vector<uint8_t>& compressed_data = compressed_data_or_error.value();

    z_stream zlib = {};
    if (inflateInit(&zlib) != Z_OK) {
      DLOG(ERROR) << "Failed to initialize zlib";
      return false;
    }

    decompress_buffer_.resize(length_in_bytes_);
    zlib.next_in = compressed_data.data();
    zlib.avail_in = compressed_data.size();
    zlib.next_out = decompress_buffer_.data();
    zlib.avail_out = decompress_buffer_.size();

    int rv = inflate(&zlib, Z_FINISH);
    inflateEnd(&zlib);

    if (rv != Z_STREAM_END) {
      DLOG(ERROR) << "Failed to decompress zlib data, error = " << rv;
      return false;
    }

    did_decompress_ = true;
  }

  return CopyOutDecompressed(buf, bytes_read);
}

bool UDIFBlockChunkReadStream::HandleBZ2(base::span<uint8_t> buf,
                                         size_t* bytes_read) {
  if (!did_decompress_) {
    auto compressed_data_or_error = ReadCompressedData();
    if (!compressed_data_or_error.has_value()) {
      return false;
    }
    std::vector<uint8_t>& compressed_data = compressed_data_or_error.value();

    bz_stream bz = {};
    if (BZ2_bzDecompressInit(&bz, 0, 0) != BZ_OK) {
      DLOG(ERROR) << "Failed to initialize bzlib";
      return false;
    }

    decompress_buffer_.resize(length_in_bytes_);
    bz.next_in = reinterpret_cast<char*>(compressed_data.data());
    bz.avail_in = compressed_data.size();
    bz.next_out = reinterpret_cast<char*>(decompress_buffer_.data());
    bz.avail_out = decompress_buffer_.size();

    int rv = BZ2_bzDecompress(&bz);
    BZ2_bzDecompressEnd(&bz);

    if (rv != BZ_STREAM_END) {
      DLOG(ERROR) << "Failed to decompress BZ2 data, error = " << rv;
      return false;
    }

    did_decompress_ = true;
  }

  return CopyOutDecompressed(buf, bytes_read);
}

std::optional<std::vector<uint8_t>>
UDIFBlockChunkReadStream::ReadCompressedData() {
  std::vector<uint8_t> data;
  data.resize(chunk_->compressed_length);

  if (stream_->Seek(chunk_->compressed_offset, SEEK_SET) == -1) {
    return std::nullopt;
  }

  if (!stream_->ReadExact(data)) {
    return std::nullopt;
  }
  return data;
}

}  // namespace

}  // namespace dmg
}  // namespace safe_browsing
