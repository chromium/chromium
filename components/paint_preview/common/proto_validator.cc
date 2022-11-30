// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/proto_validator.h"

namespace paint_preview {

namespace {

bool RectProtoValid(const RectProto& rect) {
  return rect.has_x() && rect.has_y() && rect.has_width() && rect.has_height();
}

bool LinkDataProtoValid(const LinkDataProto& link_data) {
  return link_data.has_rect() && link_data.has_url() &&
         RectProtoValid(link_data.rect());
}

bool ContentIdEmbeddingTokenPairProtoValid(
    const ContentIdEmbeddingTokenPairProto& content_id_embedding_token_pair) {
  return content_id_embedding_token_pair.has_content_id() &&
         content_id_embedding_token_pair.has_embedding_token_low() &&
         content_id_embedding_token_pair.has_embedding_token_high();
}

bool PaintPreviewFrameProtoValid(
    const PaintPreviewFrameProto& paint_preview_frame) {
  if (!paint_preview_frame.has_embedding_token_low() ||
      !paint_preview_frame.has_embedding_token_high() ||
      !paint_preview_frame.has_is_main_frame()) {
    return false;
  }

  for (const LinkDataProto& link_data : paint_preview_frame.links()) {
    if (!LinkDataProtoValid(link_data)) {
      return false;
    }
  }

  for (const ContentIdEmbeddingTokenPairProto& content_id_embedding_token_pair :
       paint_preview_frame.content_id_to_embedding_tokens()) {
    if (!ContentIdEmbeddingTokenPairProtoValid(
            content_id_embedding_token_pair)) {
      return false;
    }
  }
  return true;
}

bool MetadataProtoValid(const MetadataProto& metadata) {
  return metadata.has_url();
}

}  // namespace

bool PaintPreviewProtoValid(const PaintPreviewProto& paint_preview) {
  if (!paint_preview.has_metadata() || !paint_preview.has_root_frame() ||
      !MetadataProtoValid(paint_preview.metadata()) ||
      !PaintPreviewFrameProtoValid(paint_preview.root_frame())) {
    return false;
  }
  for (const PaintPreviewFrameProto& subframe : paint_preview.subframes()) {
    if (!PaintPreviewFrameProtoValid(subframe)) {
      return false;
    }
  }
  return true;
}

}  // namespace paint_preview
