// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/proto_validator.h"

#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

namespace {

// Generate a fully populated valid proto.
PaintPreviewProto CreatePopulatedValidProto() {
  PaintPreviewProto proto;

  auto* metadata = proto.mutable_metadata();
  metadata->set_url("https://www.example.com/");
  metadata->set_version(1);

  auto* root_frame = proto.mutable_root_frame();
  root_frame->set_embedding_token_low(12345);
  root_frame->set_embedding_token_high(67890);
  root_frame->set_is_main_frame(true);
  root_frame->set_scroll_offset_x(50);
  root_frame->set_scroll_offset_y(100);
  root_frame->set_file_path("/foo/bar");

  auto* link = root_frame->add_links();
  link->set_url("https://www.chromium.org/");

  auto* rect = link->mutable_rect();
  rect->set_x(1);
  rect->set_y(2);
  rect->set_width(3);
  rect->set_height(4);

  const int subframe_low = 654;
  const int subframe_high = 7321981;
  auto* pair = root_frame->add_content_id_to_embedding_tokens();
  pair->set_content_id(1);
  pair->set_embedding_token_low(subframe_low);
  pair->set_embedding_token_high(subframe_high);

  auto* subframe = proto.add_subframes();
  subframe->set_embedding_token_low(subframe_low);
  subframe->set_embedding_token_high(subframe_high);
  subframe->set_is_main_frame(false);
  return proto;
}

}  // namespace

// Rect validation fails if the rect is missing any dimension.
TEST(PaintPreviewProtoValid, RectValidation) {
  auto proto = CreatePopulatedValidProto();
  auto* root_frame = proto.mutable_root_frame();
  auto* link_data = root_frame->mutable_links(0);
  auto* rect = link_data->mutable_rect();
  rect->clear_x();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  link_data = root_frame->mutable_links(0);
  rect = link_data->mutable_rect();
  rect->clear_y();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  link_data = root_frame->mutable_links(0);
  rect = link_data->mutable_rect();
  rect->clear_width();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  link_data = root_frame->mutable_links(0);
  rect = link_data->mutable_rect();
  rect->clear_height();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

// Link validation fails if it is missing a url or rect.
TEST(PaintPreviewProtoValid, LinkDataValidation) {
  auto proto = CreatePopulatedValidProto();
  auto* root_frame = proto.mutable_root_frame();
  auto* link_data = root_frame->mutable_links(0);
  link_data->clear_url();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  link_data = root_frame->mutable_links(0);
  link_data->clear_rect();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

// Content ID and embedding token pair validation should fail if any field is
// missing.
TEST(PaintPreviewProtoValid, ContentIdEmbeddingTokenPairValidation) {
  auto proto = CreatePopulatedValidProto();
  auto* root_frame = proto.mutable_root_frame();
  auto* pair = root_frame->mutable_content_id_to_embedding_tokens(0);
  pair->clear_content_id();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  pair = root_frame->mutable_content_id_to_embedding_tokens(0);
  pair->clear_embedding_token_low();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  pair = root_frame->mutable_content_id_to_embedding_tokens(0);
  pair->clear_embedding_token_high();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

// Frame validation should fail if the embedding token or main frame fields are
// missing.
TEST(PaintPreviewProtoValid, PaintPreviewFrameProtoValidation) {
  auto proto = CreatePopulatedValidProto();
  auto* root_frame = proto.mutable_root_frame();
  root_frame->clear_embedding_token_low();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  root_frame->clear_embedding_token_high();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  root_frame->clear_embedding_token_high();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  root_frame = proto.mutable_root_frame();
  root_frame->clear_is_main_frame();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

// Metadata validation fails if the url is missing.
TEST(PaintPreviewProtoValid, MetadataValidation) {
  auto proto = CreatePopulatedValidProto();
  auto* metadata = proto.mutable_metadata();
  metadata->clear_url();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

// Proto validation should succeed unless the root frame, or metadata is missing
// or if a subframe is invalid.
TEST(PaintPreviewProtoValid, PaintPreviewValidation) {
  auto proto = CreatePopulatedValidProto();
  EXPECT_TRUE(PaintPreviewProtoValid(proto));

  proto.clear_metadata();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  proto.clear_root_frame();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));

  proto = CreatePopulatedValidProto();
  proto.add_subframes();
  EXPECT_FALSE(PaintPreviewProtoValid(proto));
}

}  // namespace paint_preview
