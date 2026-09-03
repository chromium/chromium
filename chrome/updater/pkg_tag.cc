// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/pkg_tag.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <ranges>
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "chrome/updater/certificate_tag.h"
#include "chrome/updater/tag.h"
#include "services/data_decoder/public/cpp/xml_dom.h"
#include "third_party/zlib/zlib.h"

namespace updater::tagging {

std::ostream& operator<<(std::ostream& os, PkgTagError error) {
  switch (error) {
    case PkgTagError::kBufferTooSmall:
      return os << "kBufferTooSmall";
    case PkgTagError::kInvalidXarMagic:
      return os << "kInvalidXarMagic";
    case PkgTagError::kInvalidXarHeader:
      return os << "kInvalidXarHeader";
    case PkgTagError::kInvalidTocSize:
      return os << "kInvalidTocSize";
    case PkgTagError::kTocDecompressionFailed:
      return os << "kTocDecompressionFailed";
    case PkgTagError::kInvalidTocXml:
      return os << "kInvalidTocXml";
    case PkgTagError::kInvalidDescriptorNode:
      return os << "kInvalidDescriptorNode";
    case PkgTagError::kIntegerOverflow:
      return os << "kIntegerOverflow";
    case PkgTagError::kPayloadOutOfBounds:
      return os << "kPayloadOutOfBounds";
    case PkgTagError::kInvalidTrailer:
      return os << "kInvalidTrailer";
  }
}

namespace {

// Format defined in https://en.wikipedia.org/wiki/Xar_(archiver).
struct XarHeader {
  uint16_t size = 0;
  uint16_t version = 0;
  uint64_t toc_length_compressed = 0;
  uint64_t toc_length_uncompressed = 0;
  uint32_t cksum_alg = 0;
};

constexpr std::array<uint8_t, 4> kXarHeaderMagic = {'x', 'a', 'r', '!'};
constexpr size_t kXarHeaderMagicSize = sizeof(kXarHeaderMagic);
constexpr size_t kXarHeaderOnDiskSize = 24;
constexpr size_t kMinXarHeaderSize = kXarHeaderOnDiskSize + kXarHeaderMagicSize;

constexpr size_t kPackageTrailerSize = 16;
constexpr std::array<uint8_t, 4> kPackageTrailerMagic = {'t', '8', 'l', 'r'};
constexpr size_t kPackageTrailerMagicSize = sizeof(kPackageTrailerMagic);

enum class PackageTrailerType : uint16_t {
  kInvalid = 0,
  kTerminator = 1,
  kTicket = 2,
};

// Defined in Security.framework (registerStapledTicketInPackage) and documented
// in https://mothersruin.com/software/Archaeology/reverse/tickets.html.
struct PackageTrailer {
  uint16_t version = 0;
  PackageTrailerType type = PackageTrailerType::kInvalid;
  uint32_t length = 0;

  bool Initialize(base::SpanReader<const uint8_t>& reader) {
    uint16_t temp_type = 0;
    if (!reader.ReadU16LittleEndian(version) || version != 1 ||
        !reader.ReadU16LittleEndian(temp_type) ||
        !reader.ReadU32LittleEndian(length)) {
      version = length = 0;
      type = PackageTrailerType::kInvalid;
      return false;
    }
    type = static_cast<PackageTrailerType>(temp_type);
    return true;
  }

  bool ParsePackageTrailer(base::span<const uint8_t> trailer_span) {
    if (trailer_span.size() < kPackageTrailerSize) {
      return false;
    }
    base::SpanReader reader(trailer_span);
    auto magic = reader.Read<kPackageTrailerMagicSize>();
    if (!magic || !std::ranges::equal(*magic, kPackageTrailerMagic)) {
      return false;
    }
    if (!Initialize(reader)) {
      return false;
    }
    return true;
  }
};

// Maximum permitted size (100 MiB) for the uncompressed XAR Table of Contents
// XML to prevent decompression bomb / denial-of-service issues.
constexpr uint64_t kMaxDecompressedXarTocSize = 100 * 1024 * 1024;

base::expected<XarHeader, PkgTagError> ParseXarHeader(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kMinXarHeaderSize) {
    return base::unexpected(PkgTagError::kBufferTooSmall);
  }
  base::SpanReader reader(buffer);
  auto magic = reader.Read<kXarHeaderMagicSize>();
  if (!magic || !std::ranges::equal(*magic, kXarHeaderMagic)) {
    return base::unexpected(PkgTagError::kInvalidXarMagic);
  }
  XarHeader header;
  if (!reader.ReadU16BigEndian(header.size) ||
      !reader.ReadU16BigEndian(header.version) ||
      !reader.ReadU64BigEndian(header.toc_length_compressed) ||
      !reader.ReadU64BigEndian(header.toc_length_uncompressed) ||
      !reader.ReadU32BigEndian(header.cksum_alg)) {
    return base::unexpected(PkgTagError::kInvalidXarHeader);
  }
  if (header.size < kMinXarHeaderSize || header.version != 1) {
    return base::unexpected(PkgTagError::kInvalidXarHeader);
  }
  return header;
}

base::expected<std::string, PkgTagError> DecompressXarToc(
    base::span<const uint8_t> buffer,
    const XarHeader& header) {
  if (header.toc_length_uncompressed == 0 ||
      header.toc_length_uncompressed > kMaxDecompressedXarTocSize) {
    return base::unexpected(PkgTagError::kInvalidTocSize);
  }
  size_t toc_compressed_size = 0;
  size_t toc_uncompressed_size = 0;
  if (!base::CheckedNumeric<size_t>(header.toc_length_compressed)
           .AssignIfValid(&toc_compressed_size) ||
      !base::CheckedNumeric<size_t>(header.toc_length_uncompressed)
           .AssignIfValid(&toc_uncompressed_size)) {
    return base::unexpected(PkgTagError::kInvalidTocSize);
  }
  if (buffer.size() < header.size + toc_compressed_size) {
    return base::unexpected(PkgTagError::kBufferTooSmall);
  }

  auto compressed_toc = buffer.subspan(header.size, toc_compressed_size);
  std::string decompressed_toc(toc_uncompressed_size, '\0');
  uLongf dest_len = base::checked_cast<uLongf>(toc_uncompressed_size);
  int zerr = uncompress(reinterpret_cast<Bytef*>(decompressed_toc.data()),
                        &dest_len, compressed_toc.data(),
                        base::checked_cast<uLong>(compressed_toc.size()));
  if (zerr != Z_OK || dest_len != toc_uncompressed_size) {
    return base::unexpected(PkgTagError::kTocDecompressionFailed);
  }
  return decompressed_toc;
}

// GetElementText accepts element nodes that contain exactly one text or cdata
// child and nothing else.
std::optional<std::string_view> GetElementText(
    const data_decoder::xml::Node& element) {
  if (element.GetType() != data_decoder::xml::Node::Type::kElement) {
    return std::nullopt;
  }
  const std::vector<std::unique_ptr<data_decoder::xml::Node>>* children =
      element.GetChildren();
  if (!children || children->size() != 1) {
    return std::nullopt;
  }
  const auto& child = (*children)[0];
  if (child->GetType() != data_decoder::xml::Node::Type::kText &&
      child->GetType() != data_decoder::xml::Node::Type::kCdata) {
    return std::nullopt;
  }
  return child->GetTextContent();
}

std::optional<uint64_t> ParseElementUint64(
    const data_decoder::xml::Node& element) {
  std::optional<std::string_view> text = GetElementText(element);
  if (!text) {
    return std::nullopt;
  }
  uint64_t val = 0;
  if (!base::StringToUint64(base::TrimWhitespaceASCII(*text, base::TRIM_ALL),
                            &val)) {
    return std::nullopt;
  }
  return val;
}

// TODO(crbug.com/554182344): Update this implementation to use proper node
// querying behavior once the library we rely on implements such a feature.
base::expected<uint64_t, PkgTagError> ParseMaxHeapExtent(
    std::string_view toc_xml) {
  base::expected<data_decoder::xml::Document, std::string> doc =
      data_decoder::xml::Document::FromUtf8(toc_xml);
  if (!doc.has_value() || !doc->GetRoot()) {
    return base::unexpected(PkgTagError::kInvalidTocXml);
  }

  uint64_t max_extent = 0;
  std::stack<const data_decoder::xml::Node*> node_stack;
  node_stack.push(doc->GetRoot());
  while (!node_stack.empty()) {
    const data_decoder::xml::Node* current = node_stack.top();
    node_stack.pop();

    const std::vector<std::unique_ptr<data_decoder::xml::Node>>* children =
        current->GetChildren();
    if (!children) {
      return base::unexpected(PkgTagError::kInvalidTocXml);
    }
    for (const auto& child : *children) {
      if (child->GetType() == data_decoder::xml::Node::Type::kElement) {
        node_stack.push(child.get());
      }
    }

    const auto* local_name = current->GetLocalName();
    if (!local_name || (*local_name != "data" && *local_name != "checksum")) {
      continue;
    }

    std::optional<uint64_t> offset;
    std::optional<uint64_t> size;
    std::optional<uint64_t> length;

    for (const auto& child : *children) {
      if (child->GetType() != data_decoder::xml::Node::Type::kElement) {
        continue;
      }
      const auto* child_name = child->GetLocalName();
      if (!child_name) {
        continue;
      }

      if (*child_name == "offset") {
        if (offset.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
        offset = ParseElementUint64(*child);
        if (!offset.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
      } else if (*child_name == "size") {
        if (size.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
        size = ParseElementUint64(*child);
        if (!size.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
      } else if (*child_name == "length") {
        if (length.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
        length = ParseElementUint64(*child);
        if (!length.has_value()) {
          return base::unexpected(PkgTagError::kInvalidDescriptorNode);
        }
      }
    }

    // Empty tags (e.g. `<data/>` for directories or 0-byte files) have no heap
    // allocation.
    if (!offset && !size && !length) {
      continue;
    }

    // Both <data> and <checksum> require <offset> and <size>.
    if (!offset || !size) {
      return base::unexpected(PkgTagError::kInvalidDescriptorNode);
    }

    // <data> elements must specify <length> (compressed size in the heap).
    if (*local_name == "data" && !length) {
      return base::unexpected(PkgTagError::kInvalidDescriptorNode);
    }

    const uint64_t amount = (*local_name == "data") ? *length : *size;
    uint64_t extent = 0;
    if (!base::CheckAdd(*offset, amount).AssignIfValid(&extent)) {
      return base::unexpected(PkgTagError::kIntegerOverflow);
    }
    max_extent = std::max(max_extent, extent);
  }

  return max_extent;
}

base::expected<size_t, PkgTagError> GetXarPayloadEnd(
    base::span<const uint8_t> buffer) {
  base::expected<XarHeader, PkgTagError> header = ParseXarHeader(buffer);
  if (!header.has_value()) {
    return base::unexpected(header.error());
  }
  base::expected<std::string, PkgTagError> toc =
      DecompressXarToc(buffer, *header);
  if (!toc.has_value()) {
    return base::unexpected(toc.error());
  }
  base::expected<uint64_t, PkgTagError> max_heap_extent =
      ParseMaxHeapExtent(*toc);
  if (!max_heap_extent.has_value()) {
    return base::unexpected(max_heap_extent.error());
  }

  size_t payload_end = 0;
  if (!base::CheckAdd(header->size, header->toc_length_compressed,
                      *max_heap_extent)
           .AssignIfValid(&payload_end)) {
    return base::unexpected(PkgTagError::kIntegerOverflow);
  }

  if (payload_end > buffer.size()) {
    return base::unexpected(PkgTagError::kPayloadOutOfBounds);
  }
  return payload_end;
}

// Returns the starting offset of the notarization trailer. If no trailer is
// found (unstapled PKG), it returns the buffer size. If a trailer is found, it
// marks the logical end of the file.
//
// The trailer is parsed backwards from the end of the file until a terminator
// is found. If any other invalid state is observed, an error is returned.
base::expected<size_t, PkgTagError> FindNotarizationTrailerStart(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kPackageTrailerSize) {
    return buffer.size();
  }

  size_t pos = buffer.size() - kPackageTrailerSize;
  PackageTrailer trailer;
  if (!trailer.ParsePackageTrailer(buffer.subspan(pos, kPackageTrailerSize))) {
    return buffer.size();
  }

  // This is terminating a list that starts at the end of the file and goes
  // backwards; only one trailer type is currently known but the format is
  // designed to support more; tickets are applied by an Apple server so we
  // need forward-compatibility since we can't predict whether they will
  // change the format without bothering to patch the OS
  while (trailer.type != PackageTrailerType::kTerminator) {
    size_t prev_trailer_size = 0;
    if (!base::CheckAdd(trailer.length, kPackageTrailerSize)
             .AssignIfValid(&prev_trailer_size) ||
        pos < prev_trailer_size) {
      return base::unexpected(PkgTagError::kInvalidTrailer);
    }
    pos -= prev_trailer_size;

    if (!trailer.ParsePackageTrailer(
            buffer.subspan(pos, kPackageTrailerSize))) {
      return base::unexpected(PkgTagError::kInvalidTrailer);
    }
  }
  return pos;
}

}  // namespace

class PkgBinary : public BinaryInterface {
 public:
  explicit PkgBinary(base::span<const uint8_t> contents,
                     size_t xar_payload_end,
                     size_t trailer_start_offset)
      : contents_(std::from_range, contents),
        xar_payload_end_(xar_payload_end),
        trailer_start_offset_(trailer_start_offset) {
    CHECK_LE(xar_payload_end_, trailer_start_offset_);
    CHECK_LE(trailer_start_offset_, contents_.size());

    base::span<const uint8_t> tag_region = base::span(contents_).subspan(
        xar_payload_end_, trailer_start_offset_ - xar_payload_end_);
    ReadTagResult result = ReadTagAndOffset(tag_region);

    if (const auto* valid = std::get_if<ValidTag>(&result)) {
      tag_ = GetTagFromTagString(valid->data);
      tag_offset_ = xar_payload_end_ + valid->offset;
      old_tag_size_ = trailer_start_offset_ - xar_payload_end_;
    } else if (const auto* invalid = std::get_if<InvalidTag>(&result)) {
      tag_offset_ = xar_payload_end_ + invalid->offset;
      old_tag_size_ = kTagMagicUtf8.size() + 2;
    } else if (std::get_if<NoTagFound>(&result)) {
      tag_offset_ = xar_payload_end_ + 0;
      old_tag_size_ = 0;
    }

    CHECK(!tag_offset_ ||
          (trailer_start_offset_ - *tag_offset_) >= old_tag_size_)
        << "Tag found where it doesn't fit; bug in PkgBinary::ctor";
  }

  std::optional<std::vector<uint8_t>> tag() const override { return tag_; }

  std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) override {
    // TODO(crbug.com/554180061): Investigate whether the propagated
    // pre-existing behavior to pad the excess tag data with zeroes is correct.
    // The better solution may be to shrink the tag data to the smaller of the
    // two values.
    size_t tag_data_size = std::max(tag.size(), old_tag_size_);
    size_t trailer_size = contents_.size() - trailer_start_offset_;
    size_t final_size = xar_payload_end_ + tag_data_size + trailer_size;

    std::vector<uint8_t> new_contents;
    new_contents.reserve(final_size);

    new_contents.append_range(base::span(contents_).first(xar_payload_end_));
    new_contents.append_range(tag);
    if (tag.size() < tag_data_size) {
      new_contents.insert(new_contents.end(), tag_data_size - tag.size(), 0);
    }
    new_contents.append_range(
        base::span(contents_).subspan(trailer_start_offset_));

    return new_contents;
  }

 private:
  std::vector<uint8_t> contents_;
  size_t xar_payload_end_ = 0;
  size_t trailer_start_offset_ = 0;
  std::optional<std::vector<uint8_t>> tag_;
  std::optional<size_t> tag_offset_;
  size_t old_tag_size_ = 0;
};

std::unique_ptr<BinaryInterface> CreatePkgBinary(
    base::span<const uint8_t> contents) {
  base::expected<size_t, PkgTagError> xar_payload_end =
      GetXarPayloadEnd(contents);
  if (!xar_payload_end.has_value()) {
    LOG(ERROR) << "Failed to parse XAR archive in PKG: "
               << xar_payload_end.error();
    return nullptr;
  }
  base::expected<size_t, PkgTagError> trailer_start =
      FindNotarizationTrailerStart(contents);
  if (!trailer_start.has_value()) {
    LOG(ERROR) << "Failed to parse notarization trailer in PKG: "
               << trailer_start.error();
    return nullptr;
  }
  if (*xar_payload_end > *trailer_start || *trailer_start > contents.size()) {
    LOG(ERROR) << "Invalid offsets detected in PKG.";
    return nullptr;
  }
  return std::make_unique<PkgBinary>(contents, *xar_payload_end,
                                     *trailer_start);
}

}  // namespace updater::tagging
