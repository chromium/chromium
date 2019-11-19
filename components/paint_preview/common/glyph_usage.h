// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_GLYPH_USAGE_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_GLYPH_USAGE_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"

namespace paint_preview {

// Base class for tracking glyph usage.
class GlyphUsage {
 public:
  GlyphUsage();
  // Range of glyph ids.
  // Note: at present |first| must be 1 as we don't support offset subset codes.
  GlyphUsage(uint16_t first, uint16_t last);
  virtual ~GlyphUsage();

  virtual void Set(uint16_t glyph_id) = 0;
  virtual bool IsSet(uint16_t glyph_id) const = 0;

  // Executes |callback| with the glyph id of each glyph that is set.
  virtual void ForEach(
      const base::RepeatingCallback<void(uint16_t)>& callback) const = 0;

  bool ShouldSubset() const { return first_ < last_; }
  uint16_t First() const { return first_; }
  uint16_t Last() const { return last_; }

  GlyphUsage& operator=(GlyphUsage&& other) noexcept;
  GlyphUsage(GlyphUsage&& other) noexcept;

 private:
  uint16_t first_;
  uint16_t last_;

  GlyphUsage(const GlyphUsage&) = delete;
  GlyphUsage& operator=(const GlyphUsage&) = delete;
};

// An implementation of GlyphUsage that works well for densely set glyphs.
// Usecases:
// - Primary language
// - Pre-subsetted fonts
class DenseGlyphUsage : public GlyphUsage {
 public:
  DenseGlyphUsage();
  DenseGlyphUsage(uint16_t num_glyphs);
  ~DenseGlyphUsage() override;

  void Set(uint16_t glyph_id) override;
  bool IsSet(uint16_t glyph_id) const override;
  void ForEach(
      const base::RepeatingCallback<void(uint16_t)>& callback) const override;

  DenseGlyphUsage& operator=(DenseGlyphUsage&& other) noexcept;
  DenseGlyphUsage(DenseGlyphUsage&& other) noexcept;

 private:
  std::vector<bool> bitset_;

  DenseGlyphUsage(const DenseGlyphUsage&) = delete;
  DenseGlyphUsage& operator=(const DenseGlyphUsage&) = delete;
};

// An implementation of GlyphUsage that works well for sparsely set glyphs.
// Usecases:
// - Non-subsetted CJK fonts
// - Emoji
// - Large glyph counts fonts with low glyph usage
// - Non-primary language
class SparseGlyphUsage : public GlyphUsage {
 public:
  SparseGlyphUsage();
  SparseGlyphUsage(uint16_t num_glyphs);
  ~SparseGlyphUsage() override;

  void Set(uint16_t glyph_id) override;
  bool IsSet(uint16_t glyph_id) const override;
  void ForEach(
      const base::RepeatingCallback<void(uint16_t)>& callback) const override;

  SparseGlyphUsage& operator=(SparseGlyphUsage&& other) noexcept;
  SparseGlyphUsage(SparseGlyphUsage&& other) noexcept;

 private:
  base::flat_set<uint16_t> glyph_ids_;

  SparseGlyphUsage(const SparseGlyphUsage&) = delete;
  SparseGlyphUsage& operator=(const SparseGlyphUsage&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_GLYPH_USAGE_H_
