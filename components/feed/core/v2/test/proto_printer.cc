// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/proto_printer.h"

#include <sstream>
#include <type_traits>
#include "base/json/string_escape.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/protocol_translator.h"

namespace feed {
namespace {}  // namespace

template <typename T, typename S = void>
struct IsFieldSetHelper {
  // Returns false if the |v| is a zero-value.
  static bool IsSet(const T& v) { return true; }
};
template <typename T>
struct IsFieldSetHelper<T, std::enable_if_t<std::is_scalar<T>::value>> {
  static bool IsSet(const T& v) { return !!v; }
};
template <>
struct IsFieldSetHelper<std::string> {
  static bool IsSet(const std::string& v) { return !v.empty(); }
};
template <typename T>
struct IsFieldSetHelper<google::protobuf::RepeatedPtrField<T>> {
  static bool IsSet(const google::protobuf::RepeatedPtrField<T>& v) {
    return !v.empty();
  }
};
template <typename T>
bool IsFieldSet(const T& v) {
  return IsFieldSetHelper<T>::IsSet(v);
}

class TextProtoPrinter {
 public:
  template <typename T>
  static std::string ToString(const T& v) {
    TextProtoPrinter pp;
    pp << v;
    return pp.ss_.str();
  }

 private:
  // Use partial specialization to implement field printing for repeated
  // fields and messages.
  template <typename T, typename S = void>
  struct FieldPrintHelper {
    static void Run(const std::string& name, const T& v, TextProtoPrinter* pp) {
      if (!IsFieldSet(v))
        return;
      pp->Indent();
      pp->PrintRaw(name);
      pp->PrintRaw(": ");
      *pp << v;
      pp->PrintRaw("\n");
    }
  };
  template <typename T>
  struct FieldPrintHelper<google::protobuf::RepeatedPtrField<T>> {
    static void Run(const std::string& name,
                    const google::protobuf::RepeatedPtrField<T>& v,
                    TextProtoPrinter* pp) {
      for (int i = 0; i < v.size(); ++i) {
        pp->Field(name, v[i]);
      }
    }
  };
  template <typename T>
  struct FieldPrintHelper<
      T,
      std::enable_if_t<
          std::is_base_of<google::protobuf::MessageLite, T>::value>> {
    static void Run(const std::string& name, const T& v, TextProtoPrinter* pp) {
      // Print nothing if it's empty.
      if (v.ByteSizeLong() == 0)
        return;
      pp->Indent();
      pp->PrintRaw(name);
      pp->PrintRaw(" ");
      *pp << v;
    }
  };

#define PRINT_FIELD(name) Field(#name, v.name())
#define PRINT_ONEOF(name)   \
  if (v.has_##name()) {     \
    Field(#name, v.name()); \
  }

  template <typename T>
  TextProtoPrinter& operator<<(const T& v) {
    ss_ << v;
    return *this;
  }
  TextProtoPrinter& operator<<(const std::string& v) {
    ss_ << base::GetQuotedJSONString(v);
    return *this;
  }

  TextProtoPrinter& operator<<(const feedwire::ContentId& v) {
    BeginMessage();
    PRINT_FIELD(content_domain);
    PRINT_FIELD(type);
    PRINT_FIELD(id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedwire::ClientInfo& v) {
    BeginMessage();
    PRINT_FIELD(platform_type);
    PRINT_FIELD(platform_version);
    PRINT_FIELD(app_type);
    PRINT_FIELD(app_version);
    PRINT_FIELD(locale);
    PRINT_FIELD(display_info);
    PRINT_FIELD(client_instance_id);
    PRINT_FIELD(advertising_id);
    PRINT_FIELD(device_country);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedwire::Version& v) {
    BeginMessage();
    PRINT_FIELD(major);
    PRINT_FIELD(minor);
    PRINT_FIELD(build);
    PRINT_FIELD(revision);
    PRINT_FIELD(architecture);
    PRINT_FIELD(build_type);
    PRINT_FIELD(api_version);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedwire::DisplayInfo& v) {
    BeginMessage();
    PRINT_FIELD(screen_density);
    PRINT_FIELD(screen_width_in_pixels);
    PRINT_FIELD(screen_height_in_pixels);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::Record& v) {
    BeginMessage();
    PRINT_ONEOF(stream_data);
    PRINT_ONEOF(stream_structures);
    PRINT_ONEOF(content);
    PRINT_ONEOF(local_action);
    PRINT_ONEOF(shared_state);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamData& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(next_page_token);
    PRINT_FIELD(last_added_time_millis);
    PRINT_FIELD(shared_state_id);
    PRINT_FIELD(stream_id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::Metadata& v) {
    BeginMessage();
    PRINT_FIELD(consistency_token);
    PRINT_FIELD(next_action_id);
    PRINT_FIELD(stream_schema_version);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamStructureSet& v) {
    BeginMessage();
    PRINT_FIELD(stream_id);
    PRINT_FIELD(sequence_number);
    PRINT_FIELD(structures);
    EndMessage();
    return *this;
  }

  TextProtoPrinter& operator<<(const feedstore::StreamStructure& v) {
    BeginMessage();
    PRINT_FIELD(operation);
    PRINT_FIELD(content_id);
    PRINT_FIELD(parent_id);
    PRINT_FIELD(type);
    PRINT_FIELD(content_info);
    EndMessage();
    return *this;
  }

  TextProtoPrinter& operator<<(const feedstore::DataOperation& v) {
    BeginMessage();
    PRINT_FIELD(structure);
    PRINT_FIELD(content);
    EndMessage();
    return *this;
  }

  TextProtoPrinter& operator<<(const feedstore::ContentInfo& v) {
    BeginMessage();
    PRINT_FIELD(score);
    PRINT_FIELD(availability_time_seconds);
    // PRINT_FIELD(representation_data);
    // PRINT_FIELD(offline_metadata);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::Content& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(frame);
    PRINT_FIELD(stream_id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamSharedState& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(shared_state_data);
    PRINT_FIELD(stream_id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StoredAction& v) {
    BeginMessage();
    PRINT_FIELD(id);
    PRINT_FIELD(upload_attempt_count);
    // PRINT_FIELD(action);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::StreamUpdate& v) {
    BeginMessage();
    PRINT_FIELD(updated_slices);
    PRINT_FIELD(new_shared_states);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::StreamUpdate_SliceUpdate& v) {
    BeginMessage();
    PRINT_ONEOF(slice);
    PRINT_FIELD(slice_id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::Slice& v) {
    BeginMessage();
    PRINT_ONEOF(xsurface_slice);
    PRINT_ONEOF(zero_state_slice);
    PRINT_ONEOF(loading_spinner_slice);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::ZeroStateSlice& v) {
    BeginMessage();
    PRINT_FIELD(type);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::LoadingSpinnerSlice& v) {
    BeginMessage();
    PRINT_FIELD(is_at_top);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::XSurfaceSlice& v) {
    BeginMessage();
    PRINT_FIELD(xsurface_frame);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedui::SharedState& v) {
    BeginMessage();
    PRINT_FIELD(id);
    PRINT_FIELD(xsurface_shared_state);
    EndMessage();
    return *this;
  }

  template <typename T>
  void Field(const std::string& name, const T& value) {
    FieldPrintHelper<T>::Run(name, value, this);
  }
  void BeginMessage() {
    ss_ << "{\n";
    indent_ += 2;
  }
  void EndMessage() {
    indent_ -= 2;
    Indent();
    ss_ << "}\n";
  }
  void PrintRaw(const std::string& text) { ss_ << text; }

  void Indent() {
    for (int i = 0; i < indent_; ++i)
      ss_ << ' ';
  }

  int indent_ = 0;
  std::stringstream ss_;
};  // namespace feed

std::string ToTextProto(const feedwire::ContentId& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedwire::DisplayInfo& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedwire::Version& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedwire::ClientInfo& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamData& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::Metadata& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamStructureSet& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamStructure& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::Content& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamSharedState& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StoredAction& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::Record& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::DataOperation& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedui::StreamUpdate& v) {
  return TextProtoPrinter::ToString(v);
}

std::ostream& operator<<(std::ostream& os, const StreamModelUpdateRequest& v) {
  os << "source: " << static_cast<int>(v.source) << '\n';
  os << "stream_data: " << v.stream_data;
  for (auto& content : v.content) {
    os << "content: " << content;
  }
  for (auto& shared_state : v.shared_states) {
    os << "shared_state: " << shared_state;
  }
  for (auto& stream_structure : v.stream_structures) {
    os << "stream_structure: " << stream_structure;
  }
  os << "max_structure_sequence_number: " << v.max_structure_sequence_number
     << '\n';
  return os;
}

}  // namespace feed
