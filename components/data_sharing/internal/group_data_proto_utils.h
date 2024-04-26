// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_PROTO_UTILS_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_PROTO_UTILS_H_

namespace data_sharing_pb {
class GroupData;
}  // namespace data_sharing_pb

namespace data_sharing {

struct GroupData;

data_sharing_pb::GroupData GroupDataToProto(const GroupData& group_data);

GroupData GroupDataFromProto(const data_sharing_pb::GroupData& group_data);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_PROTO_UTILS_H_
