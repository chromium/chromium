// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/glyph_usage.h"

#include "base/functional/callback.h"

namespace paint_preview {

GlyphUsage::GlyphUsage() : first_(0), last_(0) {}
GlyphUsage::GlyphUsage(uint16_t first, uint16_t last)
    : first_(first), last_(last) {
  DCHECK_EQ(first, 1U);
  DCHECK_LE(first, last);
}
GlyphUsage::~GlyphUsage() = default;

DenseGlyphUsage::DenseGlyphUsage() : GlyphUsage(), bitset_(0, false) {}
DenseGlyphUsage::DenseGlyphUsage(uint16_t num_glyphs)
    : GlyphUsage(1, num_glyphs), bitset_(num_glyphs + 1, false) {}
DenseGlyphUsage::~DenseGlyphUsage() = default;

void DenseGlyphUsage::Set(uint16_t glyph_id) {
  if (!ShouldSubset() ||
      (glyph_id != 0 && (glyph_id < First() || glyph_id > Last())))
    return;
  bitset_[glyph_id] = true;
}

bool DenseGlyphUsage::IsSet(uint16_t glyph_id) const {
  if (!ShouldSubset() ||
      (glyph_id != 0 && (glyph_id < First() || glyph_id > Last())))
    return false;
  return bitset_[glyph_id];
}

void DenseGlyphUsage::ForEach(
    const base::RepeatingCallback<void(uint16_t)>& callback) const {
  for (uint16_t i = 0; i < bitset_.size(); ++i) {
    if (bitset_[i])
      callback.Run(i);
  }
}

SparseGlyphUsage::SparseGlyphUsage() : GlyphUsage() {}
SparseGlyphUsage::SparseGlyphUsage(uint16_t num_glyphs)
    : GlyphUsage(1, num_glyphs) {}
SparseGlyphUsage::~SparseGlyphUsage() = default;

void SparseGlyphUsage::Set(uint16_t glyph_id) {
  if (!ShouldSubset() ||
      (glyph_id != 0 && (glyph_id < First() || glyph_id > Last())))
    return;
  glyph_ids_.insert(glyph_id);
}

bool SparseGlyphUsage::IsSet(uint16_t glyph_id) const {
  return glyph_ids_.count(glyph_id);
}

void SparseGlyphUsage::ForEach(
    const base::RepeatingCallback<void(uint16_t)>& callback) const {
  for (const auto& key : glyph_ids_)
    callback.Run(key);
}

}  // namespace paint_preview
