// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_annotation_table.h"

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace multistep_filter {

namespace {

namespace filter_annotations {
constexpr char kTableName[] = "filter_annotations";
constexpr char kId[] = "id";
constexpr char kTaskType[] = "task_type";
constexpr char kSourceDomain[] = "source_domain";
constexpr char kCreationTimestamp[] = "creation_timestamp";
}  // namespace filter_annotations

namespace filter_annotation_attributes {
constexpr char kTableName[] = "filter_annotation_attributes";
constexpr char kAnnotationId[] = "annotation_id";
constexpr char kKey[] = "key";
constexpr char kValue[] = "value";
}  // namespace filter_annotation_attributes

}  // namespace

FilterAnnotationTable::FilterAnnotationTable() = default;
FilterAnnotationTable::~FilterAnnotationTable() = default;

bool FilterAnnotationTable::Init(sql::Database* db) {
  CHECK(db);
  db_ = db;

  // TODO(crbug.com/491051120): Replace this manual schema creation with
  // shared SQL utilities.
  auto create_filter_annotations_table = [&]() -> bool {
    if (db_->DoesTableExist(filter_annotations::kTableName)) {
      return true;
    }
    const std::string kCreateFilterAnnotationsTableSql = base::StrCat(
        {"CREATE TABLE ", filter_annotations::kTableName, "(",
         filter_annotations::kId, " TEXT PRIMARY KEY NOT NULL,",
         filter_annotations::kTaskType, " TEXT NOT NULL,",
         filter_annotations::kSourceDomain, " TEXT NOT NULL,",
         filter_annotations::kCreationTimestamp, " INTEGER NOT NULL)"});
    return db_->Execute(kCreateFilterAnnotationsTableSql);
  };

  // Creates an index to optimize the retrieval of annotations for a specific
  // task, sorted by creation timestamp. This corresponds to the usage in
  // `GetAnnotationsForTask`.
  auto create_filter_annotations_index = [&]() -> bool {
    const std::string kCreateFilterAnnotationsIndexSql = base::StrCat(
        {"CREATE INDEX IF NOT EXISTS "
         "filter_annotations_task_type_timestamp_idx "
         "ON ",
         filter_annotations::kTableName, "(", filter_annotations::kTaskType,
         ", ", filter_annotations::kCreationTimestamp, ")"});
    return db_->Execute(kCreateFilterAnnotationsIndexSql);
  };

  auto create_filter_annotation_attributes_table = [&]() -> bool {
    if (db_->DoesTableExist(filter_annotation_attributes::kTableName)) {
      return true;
    }
    const std::string kCreateFilterAnnotationAttributesTableSql = base::StrCat(
        {"CREATE TABLE ", filter_annotation_attributes::kTableName, "(",
         filter_annotation_attributes::kAnnotationId, " TEXT NOT NULL,",
         filter_annotation_attributes::kKey, " TEXT NOT NULL,",
         filter_annotation_attributes::kValue, " TEXT NOT NULL)"});
    return db_->Execute(kCreateFilterAnnotationAttributesTableSql);
  };

  return create_filter_annotations_table() &&
         create_filter_annotation_attributes_table() &&
         create_filter_annotations_index();
}

bool FilterAnnotationTable::StoreAnnotation(
    const FilterAnnotation& annotation) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement insert_annotation(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat(
          {"INSERT INTO ", filter_annotations::kTableName, "(",
           filter_annotations::kId, ", ", filter_annotations::kTaskType, ", ",
           filter_annotations::kSourceDomain, ", ",
           filter_annotations::kCreationTimestamp, ") VALUES(?,?,?,?)"})));
  insert_annotation.BindString(0, annotation.id.AsLowercaseString());
  insert_annotation.BindString(1, annotation.task_type);
  insert_annotation.BindString(2, annotation.source_domain);
  insert_annotation.BindTime(3, annotation.creation_timestamp);

  if (!insert_annotation.Run()) {
    return false;
  }

  for (const FilterAttribute& attribute : annotation.attributes) {
    sql::Statement insert_attribute(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StrCat({"INSERT INTO ", filter_annotation_attributes::kTableName,
                      "(", filter_annotation_attributes::kAnnotationId, ", ",
                      filter_annotation_attributes::kKey, ", ",
                      filter_annotation_attributes::kValue,
                      ") VALUES(?,?,?)"})));
    insert_attribute.BindString(0, annotation.id.AsLowercaseString());
    insert_attribute.BindString(1, attribute.key);
    insert_attribute.BindString(2, attribute.value);
    if (!insert_attribute.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::vector<FilterAnnotation>
FilterAnnotationTable::GetAnnotationsForTaskSortedByCreationTimestamp(
    std::string_view task_type,
    size_t max_count) {
  std::vector<FilterAnnotation> annotations;

  sql::Statement select_annotations(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat({"SELECT ", filter_annotations::kId, ", ",
                    filter_annotations::kTaskType, ", ",
                    filter_annotations::kSourceDomain, ", ",
                    filter_annotations::kCreationTimestamp, " FROM ",
                    filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kTaskType, " = ? ORDER BY ",
                    filter_annotations::kCreationTimestamp, " DESC LIMIT ?"})));
  select_annotations.BindString(0, task_type);
  select_annotations.BindInt64(1, max_count);

  while (select_annotations.Step()) {
    std::string id_str = select_annotations.ColumnString(0);
    base::Uuid id = base::Uuid::ParseLowercase(id_str);
    if (!id.is_valid()) {
      continue;
    }

    std::string retrieved_task_type = select_annotations.ColumnString(1);
    std::string source_domain = select_annotations.ColumnString(2);
    base::Time creation_timestamp = select_annotations.ColumnTime(3);

    sql::Statement select_attributes(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StrCat({"SELECT ", filter_annotation_attributes::kKey, ", ",
                      filter_annotation_attributes::kValue, " FROM ",
                      filter_annotation_attributes::kTableName, " WHERE ",
                      filter_annotation_attributes::kAnnotationId, " = ?"})));
    select_attributes.BindString(0, id_str);

    std::vector<FilterAttribute> attributes;
    while (select_attributes.Step()) {
      attributes.emplace_back(select_attributes.ColumnString(0),
                              select_attributes.ColumnString(1));
    }

    annotations.emplace_back(id, retrieved_task_type, source_domain,
                             creation_timestamp, std::move(attributes));
  }

  if (!select_annotations.Succeeded()) {
    return {};
  }

  return annotations;
}

void FilterAnnotationTable::Shutdown() {
  db_ = nullptr;
}

}  // namespace multistep_filter
