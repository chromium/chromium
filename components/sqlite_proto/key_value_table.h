// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_PROTO_KEY_VALUE_TABLE_H_
#define COMPONENTS_SQLITE_PROTO_KEY_VALUE_TABLE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "sql/statement.h"

namespace google {
namespace protobuf {
class MessageLite;
}
}  // namespace google

namespace sqlite_proto {

namespace internal {

void BindDataToStatement(const std::string& key,
                         const google::protobuf::MessageLite& data,
                         sql::Statement* statement);

std::string GetSelectAllSql(const std::string& table_name);
std::string GetReplaceSql(const std::string& table_name);
std::string GetDeleteSql(const std::string& table_name);
std::string GetDeleteAllSql(const std::string& table_name);

}  // namespace internal

// The backend class helps perform database operations on a single table. The
// table name is passed as a constructor argument. The table schema is fixed: it
// always consists of two columns, TEXT type "key" and BLOB type "proto". The
// class doesn't manage the creation and the deletion of the table.
//
// All the functions except of the constructor must be called on a DB sequence
// of the corresponding TableManager. The preferred way to call the methods of
// this class is passing the method to TableManager::ScheduleDBTask().
//
// Example:
// manager_->ScheduleDBTask(
//     FROM_HERE,
//     base::BindOnce(&KeyValueTable<PrefetchData>::UpdateData,
//                    table_->AsWeakPtr(), key, data));
//
// TODO(crbug.com/40711306): Supporting weak pointers is a temporary measure
// mitigating a crash caused by complex lifetime requirements for KeyValueTable
// relative to the related classes. Making KeyValueTable<T> stateless instead
// could be a better way to resolve these lifetime issues in the long run.
template <typename T>
class KeyValueTable {
 public:
  explicit KeyValueTable(const std::string& table_name);
  // Virtual for testing.
  virtual ~KeyValueTable() = default;

  KeyValueTable(const KeyValueTable&) = delete;
  KeyValueTable& operator=(const KeyValueTable&) = delete;

  virtual void GetAllData(std::map<std::string, T>* data_map,
                          sql::Database* db) const;
  virtual void UpdateData(const std::string& key,
                          const T& data,
                          sql::Database* db);
  virtual void DeleteData(const std::vector<std::string>& keys,
                          sql::Database* db);
  virtual void DeleteAllData(sql::Database* db);

  base::WeakPtr<KeyValueTable<T>> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const std::string table_name_;
  base::WeakPtrFactory<KeyValueTable<T>> weak_ptr_factory_{this};
};

template <typename T>
KeyValueTable<T>::KeyValueTable(const std::string& table_name)
    : table_name_(table_name) {}

template <typename T>
void KeyValueTable<T>::GetAllData(std::map<std::string, T>* data_map,
                                  sql::Database* db) const {
  sql::Statement reader(db->GetUniqueStatement(
      ::sqlite_proto::internal::GetSelectAllSql(table_name_)));
  while (reader.Step()) {
    auto it = data_map->emplace(reader.ColumnString(0), T()).first;
    base::span<const uint8_t> blob = reader.ColumnBlob(1);
    it->second.ParseFromArray(blob.data(), blob.size());
  }
}

template <typename T>
void KeyValueTable<T>::UpdateData(const std::string& key,
                                  const T& data,
                                  sql::Database* db) {
  sql::Statement inserter(db->GetUniqueStatement(
      ::sqlite_proto::internal::GetReplaceSql(table_name_)));
  ::sqlite_proto::internal::BindDataToStatement(key, data, &inserter);
  inserter.Run();
}

template <typename T>
void KeyValueTable<T>::DeleteData(const std::vector<std::string>& keys,
                                  sql::Database* db) {
  sql::Statement deleter(db->GetUniqueStatement(
      ::sqlite_proto::internal::GetDeleteSql(table_name_)));
  for (const auto& key : keys) {
    deleter.BindString(0, key);
    deleter.Run();
    deleter.Reset(true);
  }
}

template <typename T>
void KeyValueTable<T>::DeleteAllData(sql::Database* db) {
  sql::Statement deleter(db->GetUniqueStatement(
      ::sqlite_proto::internal::GetDeleteAllSql(table_name_)));
  deleter.Run();
}

}  // namespace sqlite_proto

#endif  // COMPONENTS_SQLITE_PROTO_KEY_VALUE_TABLE_H_
