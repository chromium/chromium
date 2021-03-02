// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_

#include <ostream>
#include <string>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"

namespace feedwire {
class ActionPayload;
class ClientInfo;
class ContentId;
class DisplayInfo;
class Version;
}  // namespace feedwire
namespace feed {
struct StreamModelUpdateRequest;

std::string ToTextProto(const feedwire::ContentId& v);
std::string ToTextProto(const feedwire::Version& v);
std::string ToTextProto(const feedwire::DisplayInfo& v);
std::string ToTextProto(const feedwire::ClientInfo& v);
std::string ToTextProto(const feedwire::ActionPayload& v);
std::string ToTextProto(const feedstore::StreamData& v);
std::string ToTextProto(const feedstore::Metadata& v);
std::string ToTextProto(const feedstore::StreamStructureSet& v);
std::string ToTextProto(const feedstore::StreamStructure& v);
std::string ToTextProto(const feedstore::Content& v);
std::string ToTextProto(const feedstore::StreamSharedState& v);
std::string ToTextProto(const feedstore::StoredAction& v);
std::string ToTextProto(const feedstore::Record& v);
std::string ToTextProto(const feedstore::DataOperation& v);
std::string ToTextProto(const feedui::StreamUpdate& v);

inline std::ostream& operator<<(std::ostream& os,
                                const feedwire::ContentId& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedwire::DisplayInfo& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os, const feedwire::Version& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedwire::ClientInfo& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedwire::ActionPayload& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::StreamData& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::Metadata& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::StreamStructureSet& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::StreamStructure& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os, const feedstore::Content& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::StreamSharedState& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::StoredAction& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os, const feedstore::Record& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedstore::DataOperation& v) {
  return os << ToTextProto(v);
}
inline std::ostream& operator<<(std::ostream& os,
                                const feedui::StreamUpdate& v) {
  return os << ToTextProto(v);
}

std::ostream& operator<<(std::ostream& os, const StreamModelUpdateRequest& v);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_
