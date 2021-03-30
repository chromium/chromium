// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace history {

namespace {

// Converts the serialized categories into a vector of (category_id, weight)
// pairs.
std::vector<VisitContentAnnotations::Category> GetCategoriesFromStringColumn(
    const std::string& column_value) {
  std::vector<VisitContentAnnotations::Category> categories;

  std::vector<std::string> category_strings = base::SplitString(
      column_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& category_string : category_strings) {
    std::vector<std::string> category_parts = base::SplitString(
        category_string, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (category_parts.size() != 2)
      return {};

    VisitContentAnnotations::Category category;
    if (!base::StringToInt(category_parts[0], &category.id))
      continue;
    if (!base::StringToInt(category_parts[1], &category.weight))
      continue;
    categories.emplace_back(category);
  }
  return categories;
}

// Converts categories to something that can be stored in the database.
std::string ConvertCategoriesToStringColumn(
    const std::vector<VisitContentAnnotations::Category>& categories) {
  std::vector<std::string> serialized_categories;
  for (const auto& category : categories) {
    serialized_categories.emplace_back(
        base::StrCat({base::NumberToString(category.id), ":",
                      base::NumberToString(category.weight)}));
  }
  return base::JoinString(serialized_categories, ",");
}

}  // namespace

VisitAnnotationsDatabase::VisitAnnotationsDatabase() = default;
VisitAnnotationsDatabase::~VisitAnnotationsDatabase() = default;

bool VisitAnnotationsDatabase::InitVisitAnnotationsTables() {
  // Content Annotations table.
  if (!GetDB().DoesTableExist("content_annotations")) {
    if (!GetDB().Execute("CREATE TABLE content_annotations ("
                         "visit_id INTEGER PRIMARY KEY,"
                         "floc_protected_score DECIMAL(3, 2),"
                         "categories VARCHAR,"
                         "page_topics_model_version INTEGER)")) {
      return false;
    }
  }
  return true;
}

bool VisitAnnotationsDatabase::DropVisitAnnotationsTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE content_annotations");
}

bool VisitAnnotationsDatabase::AddContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO content_annotations "
      "(visit_id, floc_protected_score, categories, page_topics_model_version) "
      "VALUES (?,?,?,?)"));
  statement.BindInt64(0, visit_id);
  statement.BindDouble(
      1, static_cast<double>(visit_content_annotations.floc_protected_score));
  statement.BindString(
      2, ConvertCategoriesToStringColumn(visit_content_annotations.categories));
  statement.BindInt64(3, visit_content_annotations.page_topics_model_version);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit content annotations insert statement:  "
        << "visit_id = " << visit_id;
    return false;
  }

  return true;
}

bool VisitAnnotationsDatabase::UpdateContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE content_annotations SET "
                                 "floc_protected_score=?,categories=?,"
                                 "page_topics_model_version=? "
                                 "WHERE visit_id=?"));
  statement.BindDouble(
      0, static_cast<double>(visit_content_annotations.floc_protected_score));
  statement.BindString(
      1, ConvertCategoriesToStringColumn(visit_content_annotations.categories));
  statement.BindInt64(2, visit_content_annotations.page_topics_model_version);
  statement.BindInt64(3, visit_id);

  return statement.Run();
}

base::Optional<VisitContentAnnotations>
VisitAnnotationsDatabase::GetContentAnnotationsForVisit(VisitID visit_id) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT visit_id, floc_protected_score, "
                                 "categories,page_topics_model_version "
                                 "FROM content_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return base::nullopt;

  VisitID received_visit_id = statement.ColumnInt64(0);
  // We got a different visit than we asked for, something is wrong.
  DCHECK_EQ(visit_id, received_visit_id);
  if (visit_id != received_visit_id)
    return base::nullopt;

  VisitContentAnnotations content_annotations;
  content_annotations.floc_protected_score =
      static_cast<float>(statement.ColumnDouble(1));
  content_annotations.categories =
      GetCategoriesFromStringColumn(statement.ColumnString(2));
  content_annotations.page_topics_model_version = statement.ColumnInt64(3);
  return content_annotations;
}

bool VisitAnnotationsDatabase::DeleteContentAnnotationsForVisit(
    VisitID visit_id) {
  sql::Statement delete_content(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM content_annotations WHERE visit_id = ?"));
  delete_content.BindInt64(0, visit_id);

  return delete_content.Run();
}

}  // namespace history
