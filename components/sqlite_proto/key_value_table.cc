// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/key_value_table.h"

#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace sqlite_proto {

namespace internal {

void BindDataToStatement(const std::string& key,
                         const google::protobuf::MessageLite& data,
                         sql::Statement* statement) {
  statement->BindString(0, key);

  size_t size = data.ByteSizeLong();
  std::vector<uint8_t> proto_buffer(size);
  data.SerializeToArray(proto_buffer.data(), size);
  statement->BindBlob(1, proto_buffer);
}

std::string GetSelectAllSql(const std::string& table_name) {
  return base::StringPrintf("SELECT * FROM %s", table_name.c_str());
}
std::string GetReplaceSql(const std::string& table_name) {
  return base::StringPrintf("REPLACE INTO %s (key, proto) VALUES (?,?)",
                            table_name.c_str());
}
std::string GetDeleteSql(const std::string& table_name) {
  return base::StringPrintf("DELETE FROM %s WHERE key=?", table_name.c_str());
}
std::string GetDeleteAllSql(const std::string& table_name) {
  return base::StringPrintf("DELETE FROM %s", table_name.c_str());
}

}  // namespace internal
}  // namespace sqlite_proto
