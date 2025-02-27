// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/multimodal_message.h"

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace optimization_guide {

using ::google::protobuf::MessageLite;

MultimodalMessageData::MultimodalMessageData() = default;
MultimodalMessageData::MultimodalMessageData(const MultimodalMessageData&) =
    default;
MultimodalMessageData::MultimodalMessageData(MultimodalMessageData&&) = default;
MultimodalMessageData& MultimodalMessageData::operator=(
    const MultimodalMessageData&) = default;
MultimodalMessageData& MultimodalMessageData::operator=(
    MultimodalMessageData&&) = default;
MultimodalMessageData::~MultimodalMessageData() = default;

RepeatedMultimodalMessageData::RepeatedMultimodalMessageData() = default;
RepeatedMultimodalMessageData::RepeatedMultimodalMessageData(
    const RepeatedMultimodalMessageData&) = default;
RepeatedMultimodalMessageData::RepeatedMultimodalMessageData(
    RepeatedMultimodalMessageData&&) = default;
RepeatedMultimodalMessageData& RepeatedMultimodalMessageData::operator=(
    const RepeatedMultimodalMessageData&) = default;
RepeatedMultimodalMessageData& RepeatedMultimodalMessageData::operator=(
    RepeatedMultimodalMessageData&&) = default;
RepeatedMultimodalMessageData::~RepeatedMultimodalMessageData() = default;

MultimodalMessageEditView::MultimodalMessageEditView(
    MessageLite& message,
    MultimodalMessageData& overlay)
    : message_(message), overlay_(overlay) {}

MultimodalMessageEditView::~MultimodalMessageEditView() = default;

void MultimodalMessageEditView::Set(int tag, const std::string& v) {
  ProtoStatus status = SetProtoField(&message_.get(), tag, v);
  CHECK_EQ(status, ProtoStatus::kOk);
}

void MultimodalMessageEditView::Set(int tag, SkBitmap v) {
  MessageLite* nested_message = GetProtoMutableMessage(&message_.get(), tag);
  CHECK(nested_message);
  CHECK_EQ(nested_message->GetTypeName(), "optimization_guide.proto.Media");
  overlay_->images.emplace(tag, std::move(v));
}

MultimodalMessageEditView MultimodalMessageEditView::GetMutableMessage(
    int tag) {
  MessageLite* nested_message = GetProtoMutableMessage(&message_.get(), tag);
  return MultimodalMessageEditView(*nested_message, overlay_->nested[tag]);
}

RepeatedMultimodalMessageEditView
MultimodalMessageEditView::MutableRepeatedField(int tag) {
  return RepeatedMultimodalMessageEditView(*message_, tag,
                                           overlay_->repeated[tag]);
}

void MultimodalMessageEditView::CheckTypeAndMergeFrom(
    const MessageLite& other) {
  message_->CheckTypeAndMergeFrom(other);
}

MultimodalMessageReadView::MultimodalMessageReadView(
    const MessageLite& message,
    const MultimodalMessageData* overlay)
    : message_(message), overlay_(overlay) {}

MultimodalMessageReadView::MultimodalMessageReadView(const MessageLite& message)
    : message_(message), overlay_(nullptr) {}

MultimodalMessageReadView::~MultimodalMessageReadView() = default;

const SkBitmap* MultimodalMessageReadView::GetImage(
    const proto::ProtoField& proto_field) const {
  CHECK_GE(proto_field.proto_descriptors_size(), 1);
  std::optional<MultimodalMessageReadView> parent =
      GetEnclosingMessage(proto_field);
  if (!parent || !parent->overlay_) {
    // Either proto_field is not a reference to a known Media field,
    // or the field state was defined by an 'initial' message, and can't contain
    // an image.
    return nullptr;
  }
  int32_t leaf_tag =
      proto_field.proto_descriptors(proto_field.proto_descriptors_size() - 1)
          .tag_number();
  auto it = parent->overlay_->images.find(leaf_tag);
  if (it == parent->overlay_->images.end()) {
    return nullptr;
  }
  return &it->second;
}

std::optional<proto::Value> MultimodalMessageReadView::GetValue(
    const proto::ProtoField& proto_field) const {
  return GetProtoValue(*message_, proto_field);
}

std::optional<RepeatedMultimodalMessageReadView>
MultimodalMessageReadView::GetRepeated(
    const proto::ProtoField& proto_field) const {
  CHECK_GE(proto_field.proto_descriptors_size(), 1);
  std::optional<MultimodalMessageReadView> parent =
      GetEnclosingMessage(proto_field);
  if (!parent) {
    // 'proto_field' does not reference a known message field.
    return std::nullopt;
  }
  int32_t leaf_tag =
      proto_field.proto_descriptors(proto_field.proto_descriptors_size() - 1)
          .tag_number();
  if (!parent->overlay_) {
    return RepeatedMultimodalMessageReadView(*parent->message_, leaf_tag,
                                             nullptr);
  }
  auto it = parent->overlay_->repeated.find(leaf_tag);
  if (it == parent->overlay_->repeated.end()) {
    return RepeatedMultimodalMessageReadView(*parent->message_, leaf_tag,
                                             nullptr);
  }
  return RepeatedMultimodalMessageReadView(*parent->message_, leaf_tag,
                                           &it->second);
}

std::optional<MultimodalMessageReadView> MultimodalMessageReadView::GetNested(
    int32_t tag) const {
  const MessageLite* child = GetProtoMessage(&message_.get(), tag);
  if (!child) {
    // 'tag' does not reference a known message field.
    return std::nullopt;
  }
  if (!overlay_) {
    return MultimodalMessageReadView(*child, nullptr);
  }
  auto it = overlay_->nested.find(tag);
  if (it == overlay_->nested.end()) {
    return MultimodalMessageReadView(*child, nullptr);
  }
  return MultimodalMessageReadView(*child, &it->second);
}

std::optional<MultimodalMessageReadView>
MultimodalMessageReadView::GetEnclosingMessage(
    const proto::ProtoField& proto_field) const {
  std::optional<MultimodalMessageReadView> parent = *this;
  for (int i = 0; i < proto_field.proto_descriptors_size() - 1; i++) {
    parent = parent->GetNested(proto_field.proto_descriptors(i).tag_number());
    if (!parent) {
      return std::nullopt;
    }
  }
  return parent;
}

RepeatedMultimodalMessageEditView::RepeatedMultimodalMessageEditView(
    MessageLite& parent,
    int32_t tag,
    RepeatedMultimodalMessageData& overlay)
    : parent_(parent), tag_(tag), overlay_(overlay) {}

RepeatedMultimodalMessageEditView::~RepeatedMultimodalMessageEditView() =
    default;

MultimodalMessageEditView RepeatedMultimodalMessageEditView::Add() {
  int new_size = AddProtoMessage(&parent_.get(), tag_);
  CHECK_GT(new_size, 0);  // Zero implies an unsupported message / invalid tag.
  return Get(new_size - 1);
}
MultimodalMessageEditView RepeatedMultimodalMessageEditView::Add(
    const MessageLite& initial_message) {
  MultimodalMessageEditView result = Add();
  result.CheckTypeAndMergeFrom(initial_message);
  return result;
}

MultimodalMessageEditView RepeatedMultimodalMessageEditView::Get(int n) {
  MessageLite* message =
      GetProtoMutableRepeatedMessage(&parent_.get(), tag_, n);
  CHECK(message);
  // Overlays vector may be shorter than the actual field (due to elements
  // being appended in an initial message, or a merge).  This will default
  // initialize overlays for all of those elements, plus one for the element
  // being retrieved.
  if (static_cast<size_t>(n) >= overlay_->overlays.size()) {
    overlay_->overlays.resize(n + 1);
  }
  return MultimodalMessageEditView(*message, overlay_->overlays[n]);
}

RepeatedMultimodalMessageReadView::RepeatedMultimodalMessageReadView(
    const MessageLite& parent,
    int32_t tag,
    const RepeatedMultimodalMessageData* overlay)
    : parent_(parent), tag_(tag), overlay_(overlay) {}

RepeatedMultimodalMessageReadView::~RepeatedMultimodalMessageReadView() =
    default;

int RepeatedMultimodalMessageReadView::Size() const {
  return GetProtoRepeatedSize(&parent_.get(), tag_);
}

MultimodalMessageReadView RepeatedMultimodalMessageReadView::Get(int n) const {
  MessageLite* message = GetProtoMutableRepeatedMessage(
      const_cast<MessageLite*>(&parent_.get()), tag_, n);
  CHECK(message);
  if (!overlay_ || static_cast<size_t>(n) >= overlay_->overlays.size()) {
    // Either this field has no overlays, or messages were appended by a merge
    // and have no overlays.
    return MultimodalMessageReadView(*message, nullptr);
  }
  return MultimodalMessageReadView(*message, &overlay_->overlays[n]);
}

MultimodalMessage::MultimodalMessage() = default;
MultimodalMessage::MultimodalMessage(const MessageLite& initial_message)
    : message_(initial_message.New()) {
  message_->CheckTypeAndMergeFrom(initial_message);
}
MultimodalMessage::~MultimodalMessage() = default;

MultimodalMessage::MultimodalMessage(MultimodalMessage&&) = default;
MultimodalMessage& MultimodalMessage::operator=(MultimodalMessage&&) = default;

MultimodalMessage MultimodalMessage::Clone() {
  MultimodalMessage result;
  if (message_) {
    result = MultimodalMessage(*message_);
  }
  result.overlay_ = overlay_;
  return result;
}

MultimodalMessage MultimodalMessage::Merge(const MessageLite& other) {
  if (!message_) {
    return MultimodalMessage(other);
  }
  MultimodalMessage result = Clone();
  result.edit().CheckTypeAndMergeFrom(other);
  return result;
}

const MessageLite& MultimodalMessage::BuildProtoMessage() {
  // TODO(holte): Actually fill in media fields.
  return *message_;
}

}  // namespace optimization_guide
