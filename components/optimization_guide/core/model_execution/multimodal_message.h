// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MultimodalMessages are proto message with additional metadata associated with
// their fields, such as storing SkBitmap for image data fields, and tracking
// fields being in an incomplete state. These can only be used for proto types
// that have been registered via OnDeviceFeatureProtoRegistry.
//
// Typical usage involves creating an initial message with all of the non-media
// fields set, then creating the Multimodal message and providing media fields.
//
// Example:
//   using RequestProto = optimization_guide::proto::ExampleForTestingRequest;
//   using NestedProto = optimization_guide::proto::ExampleForTestingMessage;
//   RequestProto initial;
//   initial.mutable_nested1().set_string_value("caption");
//   MultimodalMessage request(initial);
//   request.edit()
//       .GetMutableMessage(RequestProto::kNested1FieldNumber)
//       .Set(NestedProto::kMediaFieldNumber, std::move(skbitmap1));
//   session->SetInput(std::move(request));

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MULTIMODAL_MESSAGE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MULTIMODAL_MESSAGE_H_

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "services/on_device_model/ml/chrome_ml_audio_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace optimization_guide {

struct RepeatedMultimodalMessageData;
class RepeatedMultimodalMessageReadView;
class RepeatedMultimodalMessageEditView;

enum class MultimodalType {
  kNone = 0,
  kImage,
  kAudio,
};

// Stores extra information associated with a proto message's fields.
struct MultimodalMessageData final {
  MultimodalMessageData();
  MultimodalMessageData(const MultimodalMessageData&);
  MultimodalMessageData(MultimodalMessageData&&);
  MultimodalMessageData& operator=(const MultimodalMessageData&);
  MultimodalMessageData& operator=(MultimodalMessageData&&);
  ~MultimodalMessageData();

  // Which fields are currently marked pending.
  std::set<int> pending;

  // Images stored for fields of the message.
  std::map<int, SkBitmap> images;

  // Audio data for fields of the message.
  std::map<int, ml::AudioBuffer> audio;

  // Overlay data for singular message fields.
  // The message may also have message type fields with no overlays,
  // through either an initial message or merge.
  std::map<int, MultimodalMessageData> nested;

  // Overlay data for repeated message fields.
  // The message may also have repeated message fields with no overlays,
  // through either an initial message or merge.
  std::map<int, RepeatedMultimodalMessageData> repeated;
};

// Stores extra information associated with a repeated field's messages.
struct RepeatedMultimodalMessageData final {
  RepeatedMultimodalMessageData();
  RepeatedMultimodalMessageData(const RepeatedMultimodalMessageData&);
  RepeatedMultimodalMessageData(RepeatedMultimodalMessageData&&);
  RepeatedMultimodalMessageData& operator=(
      const RepeatedMultimodalMessageData&);
  RepeatedMultimodalMessageData& operator=(RepeatedMultimodalMessageData&&);
  ~RepeatedMultimodalMessageData();

  // Overlays for each element of the repeated field.
  // May be shorter than the actual field if elements were added in an initial
  // message or by merging a message that has elements. The extra elements
  // effectively have no overlay, and default initialized values can be created
  // for them lazily.
  std::vector<MultimodalMessageData> overlays;

  // If this field is expected to have more data added later.
  bool incomplete = false;
};

// A mutable view of a MultimodalMessage (or a submessage of it).
// This is a pointer-like object, similar to a
// 'google::protobuf::MessageLite*', and it may be invalidated if the
// underlying MultimodalMessage is modified / deleted.
class MultimodalMessageEditView {
 public:
  MultimodalMessageEditView(google::protobuf::MessageLite& message,
                            MultimodalMessageData& overlay);
  ~MultimodalMessageEditView();

  // Marks a field as pending (or clears it if pending = false).
  // Substitutions will truncate where they would depend on this field.
  void MarkPending(int tag, bool pending = true);

  // Sets a string field value.
  void Set(int tag, const std::string& v);

  // Sets a media field value.
  void Set(int tag, SkBitmap v);

  // Sets a media field value.
  void Set(int tag, ml::AudioBuffer v);

  // Retrieve a message field overlay created by a previous "Set" call.
  // Mutations through the returned view will not invalidate this view, but
  // this call may invalidate other child views created from this object.
  MultimodalMessageEditView GetMutableMessage(int tag);

  // Retrieves the overlay for a repeated field, creating it if it doesn't
  // exist.
  // Mutations through the returned view will not invalidate this view, but
  // this call may invalidate other child views created from this object.
  RepeatedMultimodalMessageEditView MutableRepeatedField(int tag);

  // Merges data from 'other' into this message.
  // May invalidate child views of this object.
  void CheckTypeAndMergeFrom(const google::protobuf::MessageLite& other);

 private:
  // The underlying protobuf message.
  const raw_ref<google::protobuf::MessageLite> message_;

  // The overlay data.
  const raw_ref<MultimodalMessageData> overlay_;
};

// An immutable view of a MultimodalMessage (or a submessage of it).
// This is a pointer-like object, similar to a
// 'const google::protobuf::MessageLite*', and it may be invalidated if the
// underlying MultimodalMessage is modified / deleted.
class MultimodalMessageReadView {
 public:
  MultimodalMessageReadView(const google::protobuf::MessageLite& message,
                            const MultimodalMessageData* overlay);
  explicit MultimodalMessageReadView(
      const google::protobuf::MessageLite& message);
  ~MultimodalMessageReadView();

  // Get the type of the underlying message.
  std::string GetTypeName() const {
    return std::string(message_->GetTypeName());
  }

  // Returns true iff the field or any parent has been marked pending.
  bool IsPending(const proto::ProtoField& proto_field) const;

  // Get the type of multimodal content for a field.
  MultimodalType GetMultimodalType(const proto::ProtoField& proto_field) const;

  // Retrieve an image associated with a field.
  const SkBitmap* GetImage(const proto::ProtoField& proto_field) const;

  // Retrieve an image associated with a field.
  const ml::AudioBuffer* GetAudio(const proto::ProtoField& proto_field) const;

  // Retrieve an value stored in a proto field.
  std::optional<proto::Value> GetValue(
      const proto::ProtoField& proto_field) const;

  // Get a view for repeated message field.
  std::optional<RepeatedMultimodalMessageReadView> GetRepeated(
      const proto::ProtoField& proto_field) const;

 private:
  // Get the message for a singular message field.
  std::optional<MultimodalMessageReadView> GetNested(int tag) const;

  // Resolve all but the last tag in proto_field.
  std::optional<MultimodalMessageReadView> GetEnclosingMessage(
      const proto::ProtoField& proto_field) const;

  // The underlying protobuf message.
  raw_ref<const google::protobuf::MessageLite> message_;

  // The overlay data. This MAY be null if there is no overlay for this message.
  raw_ptr<const MultimodalMessageData> overlay_;
};

// A mutable view of a repeated field of an MultimodalMessage.
// This is a pointer-like object, similar to an MultimodalMessageEditView.
class RepeatedMultimodalMessageEditView {
 public:
  RepeatedMultimodalMessageEditView(google::protobuf::MessageLite& parent,
                                    int32_t tag,
                                    RepeatedMultimodalMessageData& overlay);
  ~RepeatedMultimodalMessageEditView();

  // Add a message to the field.
  MultimodalMessageEditView Add();
  MultimodalMessageEditView Add(
      const google::protobuf::MessageLite& initial_message);

  // Get a previously added message.
  MultimodalMessageEditView Get(int n);

  // Indicate that more content may be appended to this field later.
  // Substitutions will truncate where they would depend on whether more
  // messages are present.
  void MarkIncomplete(bool incomplete = true);

 private:
  // The underlying message that owns the repeated field.
  const raw_ref<google::protobuf::MessageLite> parent_;

  // The tag of the field within the parent message.
  const int32_t tag_;

  // The overlay data for messages in this field.
  const raw_ref<RepeatedMultimodalMessageData> overlay_;
};

// A immutable view of a repeated field of an MultimodalMessage.
// This is a pointer-like object, similar to MultimodalMessageReadView.
class RepeatedMultimodalMessageReadView {
 public:
  RepeatedMultimodalMessageReadView(
      const google::protobuf::MessageLite& parent,
      int32_t tag,
      const RepeatedMultimodalMessageData* overlay);
  ~RepeatedMultimodalMessageReadView();

  // Gets the number of elements in the field.
  int Size() const;

  // Gets a view for the nth element of the field.
  // Will crash on out of bounds access.
  MultimodalMessageReadView Get(int n) const;

  // Return whether this list was marked incomplete.
  bool IsIncomplete() const;

 private:
  // The underlying message that owns the repeated field.
  const raw_ref<const google::protobuf::MessageLite> parent_;

  // The tag of the field within the parent message.
  const int32_t tag_;

  // The overlay data for messages in this field. MAY be null if there is no
  // overlay data.
  const raw_ptr<const RepeatedMultimodalMessageData> overlay_;
};

// Object for building messages where fields can have associated media data.
class MultimodalMessage final {
 public:
  MultimodalMessage();
  explicit MultimodalMessage(
      const google::protobuf::MessageLite& initial_message);
  ~MultimodalMessage();

  MultimodalMessage(MultimodalMessage&&);
  MultimodalMessage& operator=(MultimodalMessage&&);

  // Create a copy of this message.
  MultimodalMessage Clone();

  // Constructs a new MultimodalMessage with the data from initial_message
  // merged into this one.
  MultimodalMessage Merge(const google::protobuf::MessageLite& initial_message);

  // Return the message with all media fields converted to proto.
  const google::protobuf::MessageLite& BuildProtoMessage();

  MultimodalMessageEditView edit() {
    return MultimodalMessageEditView(*message_, overlay_);
  }
  MultimodalMessageReadView read() const {
    return MultimodalMessageReadView(*message_, &overlay_);
  }

  std::string GetTypeName() const {
    return std::string(message_->GetTypeName());
  }

 private:
  std::unique_ptr<google::protobuf::MessageLite> message_;
  MultimodalMessageData overlay_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MULTIMODAL_MESSAGE_H_
