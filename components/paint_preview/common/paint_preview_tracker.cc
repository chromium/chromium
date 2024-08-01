// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/paint_preview/common/paint_preview_tracker.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

namespace {

constexpr int kMaxGlyphsForDenseGlyphUsage = 10000;

// Heuristically choose between a dense and sparse glyph usage map.
// TODO(crbug.com/40101107): Gather data to make this heuristic better.
bool ShouldUseDenseGlyphUsage(SkTypeface* typeface) {
  // DenseGlyphUsage is a bitset; it is efficient if lots of glyphs are used.
  // SparseGlyphUsage is a set; it is efficient if few glyphs are used.
  // Generally, smaller fonts have a higher percentage of used glyphs so set a
  // maximum threshold for number of glyphs before using SparseGlyphUsage.
  return typeface->countGlyphs() < kMaxGlyphsForDenseGlyphUsage;
}

}  // namespace

PaintPreviewTracker::PaintPreviewTracker(
    const base::UnguessableToken& guid,
    const std::optional<base::UnguessableToken>& embedding_token,
    bool is_main_frame)
    : guid_(guid),
      embedding_token_(embedding_token),
      is_main_frame_(is_main_frame) {}

PaintPreviewTracker::~PaintPreviewTracker() {
  DCHECK(states_.empty());
}

void PaintPreviewTracker::Save() {
  states_.push_back(matrix_);
}

void PaintPreviewTracker::SetMatrix(const SkMatrix& matrix) {
  matrix_ = matrix;
}

void PaintPreviewTracker::Restore() {
  if (states_.size() == 0) {
    DLOG(ERROR) << "No state to restore";
    return;
  }
  matrix_ = states_.back();
  states_.pop_back();
}

void PaintPreviewTracker::Concat(const SkMatrix& matrix) {
  if (matrix.isIdentity())
    return;
  matrix_.preConcat(matrix);
}

void PaintPreviewTracker::Scale(SkScalar x, SkScalar y) {
  if (x != 1 || y != 1) {
    matrix_.preScale(x, y);
  }
}

void PaintPreviewTracker::Rotate(SkScalar degrees) {
  SkMatrix m;
  m.setRotate(degrees);
  Concat(m);
}

void PaintPreviewTracker::Translate(SkScalar x, SkScalar y) {
  if (x || y) {
    matrix_.preTranslate(x, y);
  }
}

uint32_t PaintPreviewTracker::CreateContentForRemoteFrame(
    const gfx::Rect& rect,
    const base::UnguessableToken& embedding_token) {
  sk_sp<SkPicture> pic = SkPicture::MakePlaceholder(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()));
  const uint32_t content_id = pic->uniqueID();
  DCHECK(!base::Contains(picture_context_.content_id_to_embedding_token,
                         content_id));
  picture_context_.content_id_to_embedding_token[content_id] = embedding_token;
  subframe_pics_[content_id] = pic;
  return content_id;
}

void PaintPreviewTracker::AddGlyphs(const SkTextBlob* blob) {
  if (!blob)
    return;
  SkTextBlob::Iter::Run run;
  for (SkTextBlob::Iter it(*blob); it.next(&run);) {
    SkTypeface* typeface = run.fTypeface;
    // Fail fast if the number of glyphs is undetermined or 0.
    if (typeface->countGlyphs() <= 0)
      continue;
    if (!base::Contains(typeface_glyph_usage_, typeface->uniqueID())) {
      if (ShouldUseDenseGlyphUsage(typeface)) {
        typeface_glyph_usage_.insert(
            {typeface->uniqueID(),
             std::make_unique<DenseGlyphUsage>(
                 static_cast<uint16_t>(typeface->countGlyphs() - 1))});
      } else {
        typeface_glyph_usage_.insert(
            {typeface->uniqueID(),
             std::make_unique<SparseGlyphUsage>(
                 static_cast<uint16_t>(typeface->countGlyphs() - 1))});
      }
      // Always set the 0th glyph.
      typeface_glyph_usage_[typeface->uniqueID()]->Set(0U);
    }
    const uint16_t* glyphs = run.fGlyphIndices;
    for (int i = 0; i < run.fGlyphCount; ++i)
      typeface_glyph_usage_[typeface->uniqueID()]->Set(glyphs[i]);
  }
}

void PaintPreviewTracker::AnnotateLink(const GURL& url, const SkRect& rect) {
  SkRect out_rect;
  matrix_.mapRect(&out_rect, rect);
  links_.push_back(mojom::LinkData::New(
      url, gfx::Rect(out_rect.x(), out_rect.y(), out_rect.width(),
                     out_rect.height())));
}

void PaintPreviewTracker::TransformClipForFrame(uint32_t id) {
  auto pic_it = subframe_pics_.find(id);
  if (pic_it == subframe_pics_.end())
    return;

  SkRect out_rect;
  matrix_.mapRect(&out_rect, pic_it->second->cullRect());
  picture_context_.content_id_to_transformed_clip.emplace(id, out_rect);
}

void PaintPreviewTracker::CustomDataToSkPictureCallback(SkCanvas* canvas,
                                                        uint32_t content_id) {
  auto map_it = picture_context_.content_id_to_embedding_token.find(content_id);
  if (map_it == picture_context_.content_id_to_embedding_token.end())
    return;

  auto it = subframe_pics_.find(content_id);
  // DCHECK is sufficient as |subframe_pics_| has same entries as
  // |content_id_to_proxy_id|.
  CHECK(it != subframe_pics_.end(), base::NotFatalUntil::M130);

  SkRect rect = it->second->cullRect();
  SkMatrix subframe_offset = SkMatrix::Translate(rect.x(), rect.y());
  canvas->drawPicture(it->second, &subframe_offset, nullptr);
}

void PaintPreviewTracker::MoveLinks(std::vector<mojom::LinkDataPtr>* out) {
  std::move(links_.begin(), links_.end(), std::back_inserter(*out));
  links_.clear();
}

}  // namespace paint_preview
