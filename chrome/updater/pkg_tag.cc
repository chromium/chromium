// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/pkg_tag.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/updater/certificate_tag.h"
#include "chrome/updater/tag.h"

namespace updater::tagging {

static constexpr std::array<uint8_t, 4> kNotarizationTrailerMagic = {'t', '8',
                                                                     'l', 'r'};

// Length of a PKG file's notarization footer. This is the length of the
// "t8lr" sentinel followed by the 12 bytes of unknown interpretation.
static constexpr std::ptrdiff_t kNotarizationTrailerFooterLength = 16;

// Returns an iterator to the start of the Apple PKG notarization trailer at the
// end of `buffer`, or `buffer.end()` if `buffer` does not end with such.
base::span<const uint8_t>::iterator FindNotarizationTrailerStartIfAny(
    base::span<const uint8_t> buffer) {
  auto last_t8lr_subrange =
      std::ranges::find_end(buffer, kNotarizationTrailerMagic);
  auto last_t8lr = last_t8lr_subrange.begin();

  // A valid notarization trailer is always at the exact end of the file. Thus,
  // if the final "t8lr" is not `kNotarizationTrailerFooterLength` bytes from
  // the end of the file, the file is not notarized.
  if (buffer.end() - last_t8lr != kNotarizationTrailerFooterLength) {
    return buffer.end();
  }

  // Find the start of the notarization block, which also starts with t8lr.
  // This fails when the notarization data itself contains the substring
  // "t8lr".
  auto first_t8lr_subrange =
      std::ranges::find_end(std::ranges::subrange(buffer.begin(), last_t8lr),
                            kNotarizationTrailerMagic);
  return first_t8lr_subrange.begin();
}

std::string ReadTagFromPkg(base::span<const uint8_t> tail) {
  auto trailer_start = FindNotarizationTrailerStartIfAny(tail);
  return ReadTag(
      tail.first(base::checked_cast<size_t>(trailer_start - tail.begin())));
}

class PkgBinary : public BinaryInterface {
 public:
  explicit PkgBinary(base::span<const uint8_t> contents)
      : contents_(std::from_range, contents) {
    base::span<const uint8_t> contents_span = contents_;
    auto trailer_it = FindNotarizationTrailerStartIfAny(contents_span);
    trailer_start_offset_ =
        base::checked_cast<size_t>(trailer_it - contents_span.begin());

    ReadTagResult result =
        ReadTagAndOffset(contents_span.first(trailer_start_offset_));

    if (const auto* valid = std::get_if<ValidTag>(&result)) {
      tag_ = GetTagFromTagString(valid->data);
      tag_offset_ = valid->offset;
      old_tag_size_ = tag_->size();
    } else if (const auto* invalid = std::get_if<InvalidTag>(&result)) {
      tag_offset_ = invalid->offset;
      old_tag_size_ = kTagMagicUtf8.size() + 2;
    }

    CHECK(!tag_offset_ ||
          (trailer_start_offset_ - *tag_offset_) >= old_tag_size_)
        << "Tag found where it doesn't fit; bug in PkgBinary::ctor";
  }

  std::optional<std::vector<uint8_t>> tag() const override { return tag_; }

  std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) override {
    if (!tag_offset_.has_value()) {
      return InjectTag(tag);
    }

    // Overwrite the existing tag, using the length of the new tag to
    // determine the range to overwrite -- tag size refers to length of valid
    // tag data, not the entire available patch space, so it cannot be used
    // to determine the extent of the range to remove. We must preserve a
    // notarization trailer (if any), however, so we can use this as an upper
    // bound on available patch space. If the new tag is shorter than the old
    // tag's known data, trim out the entire old tag's data range (it will be
    // replaced with zeroes later).
    auto remove_start = contents_.begin() + *tag_offset_;
    size_t remove_len =
        std::min(trailer_start_offset_ - *tag_offset_, tag.size());
    if (tag.size() < old_tag_size_) {
      // If the old tag wouldn't fit, the constructor should not have found it.
      CHECK_GE(trailer_start_offset_ - *tag_offset_, old_tag_size_)
          << "Existing tag can't fit; bug in PkgBinary::ctor";
      remove_len = old_tag_size_;
    }
    auto remove_end = remove_start + remove_len;

    // If the removed range is smaller than the new tag, the file will grow to
    // allow the new tag to be inserted. If the new tag is smaller than the
    // original tag's data, fill the excess with zeroes.
    size_t final_size =
        contents_.size() - remove_len + tag.size() +
        (tag.size() < old_tag_size_ ? old_tag_size_ - tag.size() : 0);
    std::vector<uint8_t> new_contents;
    new_contents.reserve(final_size);
    new_contents.append_range(
        std::ranges::subrange(contents_.begin(), remove_start));
    new_contents.append_range(tag);
    if (tag.size() < old_tag_size_) {
      new_contents.insert(new_contents.end(), old_tag_size_ - tag.size(), 0);
    }
    if (remove_end < contents_.end()) {
      new_contents.append_range(
          std::ranges::subrange(remove_end, contents_.end()));
    }

    return new_contents;
  }

 private:
  std::vector<uint8_t> contents_;
  std::optional<std::vector<uint8_t>> tag_;
  std::optional<size_t> tag_offset_;
  size_t old_tag_size_ = 0;
  size_t trailer_start_offset_ = 0;

  std::optional<std::vector<uint8_t>> InjectTag(base::span<const uint8_t> tag) {
    size_t final_size = contents_.size() + tag.size();
    std::vector<uint8_t> new_contents;
    new_contents.reserve(final_size);
    auto trailer_start = contents_.begin() + trailer_start_offset_;
    new_contents.append_range(
        std::ranges::subrange(contents_.begin(), trailer_start));
    new_contents.append_range(tag);
    new_contents.append_range(
        std::ranges::subrange(trailer_start, contents_.end()));

    return new_contents;
  }
};

std::unique_ptr<BinaryInterface> CreatePkgBinary(
    base::span<const uint8_t> contents) {
  return std::make_unique<PkgBinary>(contents);
}

}  // namespace updater::tagging
