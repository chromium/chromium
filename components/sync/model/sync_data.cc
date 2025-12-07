// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_data.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

// TODO(crbug.com/40733890): Avoid using thread-safe refcounting, since it's
// only needed by a few (one?) browser test.
struct SyncData::InternalData
    : public base::RefCountedThreadSafe<InternalData> {
  InternalData() = default;

  ClientTagHash client_tag_hash;
  std::string non_unique_name;
  sync_pb::EntitySpecifics specifics;

 private:
  friend class base::RefCountedThreadSafe<InternalData>;
  ~InternalData() = default;
};

SyncData::SyncData() = default;

SyncData::SyncData(scoped_refptr<InternalData> ptr) : ptr_(std::move(ptr)) {}

SyncData::SyncData(const SyncData& other) = default;

SyncData::SyncData(SyncData&& other) = default;

SyncData& SyncData::operator=(const SyncData& other) = default;

SyncData& SyncData::operator=(SyncData&& other) = default;

SyncData::~SyncData() = default;

// Static.
SyncData SyncData::CreateLocalDelete(std::string_view client_tag_unhashed,
                                     DataType datatype) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(datatype, &specifics);
  return CreateLocalData(client_tag_unhashed, {}, specifics);
}

// Static.
SyncData SyncData::CreateLocalData(std::string_view client_tag_unhashed,
                                   std::string_view non_unique_title,
                                   const sync_pb::EntitySpecifics& specifics) {
  const DataType data_type = GetDataTypeFromSpecifics(specifics);
  DCHECK(IsRealDataType(data_type));

  DCHECK(!client_tag_unhashed.empty());

  SyncData data(base::MakeRefCounted<InternalData>());
  data.ptr_->client_tag_hash =
      ClientTagHash::FromUnhashed(data_type, client_tag_unhashed);
  data.ptr_->non_unique_name = non_unique_title;
  data.ptr_->specifics = specifics;
  return data;
}

// Static.
SyncData SyncData::CreateRemoteData(sync_pb::EntitySpecifics specifics,
                                    const ClientTagHash& client_tag_hash) {
  SyncData data(base::MakeRefCounted<InternalData>());
  data.ptr_->client_tag_hash = client_tag_hash;
  data.ptr_->specifics = std::move(specifics);
  DCHECK(IsRealDataType(data.GetDataType()));
  return data;
}

bool SyncData::IsValid() const {
  return ptr_ != nullptr;
}

const sync_pb::EntitySpecifics& SyncData::GetSpecifics() const {
  return ptr_->specifics;
}

DataType SyncData::GetDataType() const {
  return GetDataTypeFromSpecifics(GetSpecifics());
}

ClientTagHash SyncData::GetClientTagHash() const {
  return ptr_->client_tag_hash;
}

const std::string& SyncData::GetTitle() const {
  return ptr_->non_unique_name;
}

std::string SyncData::ToString() const {
  if (!IsValid()) {
    return "<Invalid SyncData>";
  }

  std::string type = DataTypeToDebugString(GetDataType());
  std::string specifics;
  base::JSONWriter::WriteWithOptions(EntitySpecificsToValue(GetSpecifics()),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &specifics);

  return "{ type: " + type + ", tagHash: " + GetClientTagHash().value() +
         ", title: " + GetTitle() + ", specifics: " + specifics + "}";
}

void PrintTo(const SyncData& sync_data, std::ostream* os) {
  *os << sync_data.ToString();
}

}  // namespace syncer
