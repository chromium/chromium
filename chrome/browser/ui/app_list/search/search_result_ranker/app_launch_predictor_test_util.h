// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_TEST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_TEST_UTIL_H_

#include <string>

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.pb.h"
#include "third_party/protobuf/src/google/protobuf/stubs/mathutil.h"

namespace app_list {

// Returns whether two protos are equivalent, which is defined as:
//   (1) each integer field has the same value.
//   (2) each float field has to be AlmostEqual.
//   (3) each map field should have same pairs of key-value.
//   (4) each proto field is defined recursively.
// Note:
//   (1) This function should only be used for MessageLite where reflection is
//       not supported; otherwise MessageDifferencer is preferred.
//   (2) This function will not support any proto by default; new proto (and all
//       its sub-proto) should be added with macro DEFINE_EQUIVTO_PROTO_LITE_*.
template <typename Proto>
bool EquivToProtoLite(const Proto& p1, const Proto& p2);

namespace internal {

// General templated class for implementation of EquivToProtoLite;
template <typename Proto>
class EquivToProtoLiteImpl {};

// Specialized by std::string.
template <>
class EquivToProtoLiteImpl<std::string> {
 public:
  bool operator()(const std::string& p1, const std::string& p2) {
    return p1 == p2;
  }
};

// Specialized by int32_t.
template <>
class EquivToProtoLiteImpl<int32_t> {
 public:
  bool operator()(const int32_t p1, const int32_t p2) { return p1 == p2; }
};

// Specialized by float.
template <>
class EquivToProtoLiteImpl<float> {
 public:
  bool operator()(const float p1, const float p2) {
    return google::protobuf::MathUtil::AlmostEquals(p1, p2);
  }
};

// Specialized by google::protobuf::Map.
template <typename K, typename V>
class EquivToProtoLiteImpl<google::protobuf::Map<K, V>> {
 public:
  using Map = const google::protobuf::Map<K, V>;
  bool operator()(const Map& p1, const Map& p2) {
    if (p1.size() != p2.size())
      return false;
    for (const auto& pair : p1) {
      const auto find_in_p2 = p2.find(pair.first);
      if (find_in_p2 == p2.end())
        return false;
      if (!EquivToProtoLite(pair.second, find_in_p2->second))
        return false;
    }
    return true;
  }
};

// Macro that generates a specialization for |Proto| with one field f1.
#define DEFINE_EQUIVTO_PROTO_LITE_1(Proto, f1)          \
  template <>                                           \
  class EquivToProtoLiteImpl<Proto> {                   \
   public:                                              \
    bool operator()(const Proto& t1, const Proto& t2) { \
      return EquivToProtoLite(t1.f1(), t2.f1());        \
    }                                                   \
  };

// Macro that generates a specialization for |Proto| with two fields f1, f2.
#define DEFINE_EQUIVTO_PROTO_LITE_2(Proto, f1, f2)      \
  template <>                                           \
  class EquivToProtoLiteImpl<Proto> {                   \
   public:                                              \
    bool operator()(const Proto& t1, const Proto& t2) { \
      return EquivToProtoLite(t1.f1(), t2.f1()) &&      \
             EquivToProtoLite(t1.f2(), t2.f2());        \
    }                                                   \
  };

// Macro that generates a specialization for |Proto| with three fields.
#define DEFINE_EQUIVTO_PROTO_LITE_3(Proto, f1, f2, f3)  \
  template <>                                           \
  class EquivToProtoLiteImpl<Proto> {                   \
   public:                                              \
    bool operator()(const Proto& t1, const Proto& t2) { \
      return EquivToProtoLite(t1.f1(), t2.f1()) &&      \
             EquivToProtoLite(t1.f2(), t2.f2()) &&      \
             EquivToProtoLite(t1.f3(), t2.f3());        \
    }                                                   \
  };

DEFINE_EQUIVTO_PROTO_LITE_1(FakeAppLaunchPredictorProto, rank_result);

DEFINE_EQUIVTO_PROTO_LITE_1(HourAppLaunchPredictorProto,
                            binned_frequency_table);

DEFINE_EQUIVTO_PROTO_LITE_2(HourAppLaunchPredictorProto_FrequencyTable,
                            total_counts,
                            frequency);

DEFINE_EQUIVTO_PROTO_LITE_2(SerializedMrfuAppLaunchPredictorProto,
                            num_of_trains,
                            scores);

DEFINE_EQUIVTO_PROTO_LITE_2(SerializedMrfuAppLaunchPredictorProto_Score,
                            num_of_trains_at_last_update,
                            last_score);

DEFINE_EQUIVTO_PROTO_LITE_3(AppLaunchPredictorProto,
                            fake_app_launch_predictor,
                            hour_app_launch_predictor,
                            serialized_mrfu_app_launch_predictor);

}  // namespace internal

template <typename Proto>
bool EquivToProtoLite(const Proto& p1, const Proto& p2) {
  return internal::EquivToProtoLiteImpl<Proto>()(p1, p2);
}

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_TEST_UTIL_H_
