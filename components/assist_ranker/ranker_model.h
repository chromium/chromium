// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_MODEL_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_MODEL_H_

#include <memory>
#include <string>

namespace assist_ranker {

class RankerModelProto;

class RankerModel {
 public:
  RankerModel();

  RankerModel(const RankerModel&) = delete;
  RankerModel& operator=(const RankerModel&) = delete;

  ~RankerModel();

  // Returns a new ranker model constructed from |data|.
  static std::unique_ptr<RankerModel> FromString(const std::string& data);

  const RankerModelProto& proto() const { return *proto_; }
  RankerModelProto* mutable_proto() const { return proto_.get(); }

  // Returns true if this ranker model has expired.
  bool IsExpired() const;

  const std::string& GetSourceURL() const;

  // Returns a serialization of this ranker model.
  std::string SerializeAsString() const;

 private:
  // The underlying ranker model proto.
  std::unique_ptr<RankerModelProto> proto_;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_MODEL_H_
