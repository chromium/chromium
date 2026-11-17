// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_TEXT_DOCUMENT_CONTENT_H_
#define COMPONENTS_MIRRORING_SERVICE_TEXT_DOCUMENT_CONTENT_H_

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace mirroring {

// Deterministically renders frames that resemble a tab showing a text document
// (e.g. Google Docs with 11pt text): a static toolbar at the top, a gray canvas
// and a white page containing lines of dark, anti-aliased, glyph-like strokes.
// The document area can be scrolled vertically by an arbitrary number of
// pixels, while the toolbar stays fixed, like a real document editor.
//
// Frames are I420 with neutral chroma, since text documents are almost
// entirely grayscale.
class TextDocumentContent {
 public:
  // `frame_size` is the size of the captured tab. `seed` controls the
  // generated text layout.
  TextDocumentContent(const gfx::Size& frame_size, uint32_t seed);
  TextDocumentContent(const TextDocumentContent&) = delete;
  TextDocumentContent& operator=(const TextDocumentContent&) = delete;
  ~TextDocumentContent();

  // Renders the document scrolled down by `scroll_offset` pixels. The
  // document wraps around once the end is reached.
  scoped_refptr<media::VideoFrame> Render(int scroll_offset,
                                          base::TimeDelta timestamp) const;

  // Renders into an existing I420 frame of size `frame_size()`.
  void RenderInto(int scroll_offset, media::VideoFrame& frame) const;

  const gfx::Size& frame_size() const { return frame_size_; }
  int toolbar_height() const { return toolbar_height_; }
  int document_height() const { return document_height_; }

 private:
  void GenerateDocument(uint32_t seed);

  const gfx::Size frame_size_;
  const int toolbar_height_;
  const int document_height_;

  // Luma of the static toolbar, `frame_size_.width()` x `toolbar_height_`.
  std::vector<uint8_t> toolbar_luma_;

  // Luma of the full, scrollable document,
  // `frame_size_.width()` x `document_height_`.
  std::vector<uint8_t> document_luma_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_TEXT_DOCUMENT_CONTENT_H_
