// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_database.h"

#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/crash/core/common/crash_key.h"
#include "components/database_utils/upper_bound_string.h"
#include "components/database_utils/url_converter.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/keyword_search_term_util.h"
#include "components/url_formatter/url_formatter.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace history {

const int URLDatabase::kNumURLRowFields = 9;

URLDatabase::URLEnumeratorBase::URLEnumeratorBase()
    : initialized_(false) {
}

URLDatabase::URLEnumeratorBase::~URLEnumeratorBase() = default;

URLDatabase::URLEnumerator::URLEnumerator() = default;

bool URLDatabase::URLEnumerator::GetNextURL(URLRow* r) {
  CHECK(r);
  while (statement_.Step()) {
    if (FillURLRow(statement_, r)) {
      CHECK(r->url().is_valid());
      return true;
    }
  }
  return false;
}

URLDatabase::URLDatabase()
    : has_keyword_search_terms_(false) {
}

URLDatabase::~URLDatabase() = default;

bool URLDatabase::FillURLRow(sql::Statement& s, URLRow* i) {
  DCHECK(i);

  GURL url(s.ColumnString(1));
  if (!url.is_valid()) {
    return false;
  }

  i->set_id(s.ColumnInt64(0));
  i->set_url(url);
  i->set_title(s.ColumnString16(2));
  i->set_visit_count(s.ColumnInt(3));
  i->set_typed_count(s.ColumnInt(4));
  i->set_last_visit(s.ColumnTime(5));
  i->set_hidden(s.ColumnInt(6) != 0);
  return true;
}

bool URLDatabase::MigrateKeywordsSearchTermsLowerTermColumn() {
  // Create a temporary keyword search terms table.
  if (!GetDB().Execute(
          "CREATE TABLE temp_keyword_search_terms ("
          "keyword_id INTEGER NOT NULL,"  // ID of the TemplateURL.
          "url_id INTEGER NOT NULL,"      // ID of the url.
          "term LONGVARCHAR NOT NULL,"    // The actual search term.
          // The search term, in lower case, and with whitespaces collapsed.
          "normalized_term LONGVARCHAR NOT NULL)")) {
    return false;
  }

  // Extract rows from the keyword search terms table, convert lower_term to
  // normalized_term, and insert them into the temporary table.
  sql::Statement select_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT keyword_id, url_id, lower_term, term "
                                 "FROM keyword_search_terms"));
  while (select_statement.Step()) {
    sql::Statement insert_statement(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO temp_keyword_search_terms "
        "(keyword_id, url_id, term, normalized_term) VALUES (?,?,?,?)"));
    insert_statement.BindInt64(0, select_statement.ColumnInt64(0));
    insert_statement.BindInt64(1, select_statement.ColumnInt64(1));
    insert_statement.BindString16(2, select_statement.ColumnString16(3));
    insert_statement.BindString16(
        3, base::CollapseWhitespace(select_statement.ColumnString16(2), false));
    if (!insert_statement.Run())
      return false;
  }
  if (!select_statement.Succeeded())
    return false;

  // Replace the keyword search terms table with the temporary one.
  if (!GetDB().Execute("DROP TABLE keyword_search_terms"))
    return false;
  if (!GetDB().Execute("ALTER TABLE temp_keyword_search_terms RENAME TO "
                       "keyword_search_terms")) {
    return false;
  }

  // Index the table, this is faster than creating the index first and then
  // inserting into it.
  CreateKeywordSearchTermsIndices();

  return true;
}

bool URLDatabase::GetURLRow(URLID url_id, URLRow* info) {
  // TODO(brettw) We need check for empty URLs to handle the case where
  // there are old URLs in the database that are empty that got in before
  // we added any checks. We should eventually be able to remove it
  // when all inputs are using GURL (which prohibit empty input).
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE id=?"));
  statement.BindInt64(0, url_id);

  if (statement.Step()) {
    return FillURLRow(statement, info);
  }
  return false;
}

URLID URLDatabase::GetRowForURL(const GURL& url, URLRow* info) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE url=?"));
  std::string url_string = database_utils::GurlToDatabaseUrl(url);
  statement.BindString(0, url_string);

  if (!statement.Step()) {
    return 0;  // No data.
  }

  if (info && !FillURLRow(statement, info)) {
    return 0;  // Invalid URL row.
  }

  return statement.ColumnInt64(0);
}

bool URLDatabase::UpdateURLRow(URLID url_id, const URLRow& info) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE urls SET title=?,visit_count=?,typed_count=?,last_visit_time=?,"
        "hidden=?"
      "WHERE id=?"));
  statement.BindString16(0, info.title());
  statement.BindInt(1, info.visit_count());
  statement.BindInt(2, info.typed_count());
  statement.BindTime(3, info.last_visit());
  statement.BindInt(4, info.hidden() ? 1 : 0);
  statement.BindInt64(5, url_id);

  return statement.Run() && GetDB().GetLastChangeCount() > 0;
}

URLID URLDatabase::AddURLInternal(const URLRow& info, bool is_temporary) {
  // This function is used to insert into two different tables, so we have to
  // do some shuffling. Unfortunately, we can't use the macro
  // HISTORY_URL_ROW_FIELDS because that specifies the table name which is
  // invalid in the insert syntax.
  #define ADDURL_COMMON_SUFFIX \
      " (url, title, visit_count, typed_count, "\
      "last_visit_time, hidden) "\
      "VALUES (?,?,?,?,?,?)"
  sql::Statement statement;
  if (is_temporary) {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO temp_urls" ADDURL_COMMON_SUFFIX));
  } else {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO urls" ADDURL_COMMON_SUFFIX));
  }
  #undef ADDURL_COMMON_SUFFIX

  statement.BindString(0, database_utils::GurlToDatabaseUrl(info.url()));
  statement.BindString16(1, info.title());
  statement.BindInt(2, info.visit_count());
  statement.BindInt(3, info.typed_count());
  statement.BindTime(4, info.last_visit());
  statement.BindInt(5, info.hidden() ? 1 : 0);

  if (!statement.Run()) {
    VLOG(0) << "Failed to add url " << info.url().possibly_invalid_spec()
            << " to table history.urls.";
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool URLDatabase::URLTableContainsAutoincrement() {
  // sqlite_schema has columns:
  //   type - "index" or "table".
  //   name - name of created element.
  //   tbl_name - name of element, or target table in case of index.
  //   rootpage - root page of the element in database file.
  //   sql - SQL to create the element.
  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT sql FROM sqlite_schema WHERE type = 'table' AND name = 'urls'"));

  // urls table does not exist.
  if (!statement.Step())
    return false;

  std::string urls_schema = statement.ColumnString(0);
  // We check if the whole schema contains "AUTOINCREMENT", since
  // "AUTOINCREMENT" only can be used for "INTEGER PRIMARY KEY", so we assume no
  // other columns could contain "AUTOINCREMENT".
  return urls_schema.find("AUTOINCREMENT") != std::string::npos;
}

bool URLDatabase::InsertOrUpdateURLRowByID(const URLRow& info) {
  // SQLite does not support INSERT OR UPDATE, however, it does have INSERT OR
  // REPLACE, which is feasible to use, because of the following.
  //  * Before INSERTing, REPLACE will delete all pre-existing rows that cause
  //    constraint violations. Here, we only have a PRIMARY KEY constraint, so
  //    the only row that might get deleted is an old one with the same ID.
  //  * Another difference between the two flavors is that the latter actually
  //    deletes the old row, and thus the old values are lost in columns which
  //    are not explicitly assigned new values. This is not an issue, however,
  //    as we assign values to all columns.
  //  * When rows are deleted due to constraint violations, the delete triggers
  //    may not be invoked. As of now, we do not have any delete triggers.
  // For more details, see: http://www.sqlite.org/lang_conflict.html.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO urls "
      "(id, url, title, visit_count, typed_count, last_visit_time, hidden) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, info.id());
  statement.BindString(1, database_utils::GurlToDatabaseUrl(info.url()));
  statement.BindString16(2, info.title());
  statement.BindInt(3, info.visit_count());
  statement.BindInt(4, info.typed_count());
  statement.BindTime(5, info.last_visit());
  statement.BindInt(6, info.hidden() ? 1 : 0);

  return statement.Run();
}

bool URLDatabase::DeleteURLRow(URLID id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM urls WHERE id = ?"));
  statement.BindInt64(0, id);

  if (!statement.Run())
    return false;

  // And delete any keyword visits.
  return !has_keyword_search_terms_ || DeleteKeywordSearchTermForURL(id);
}

bool URLDatabase::CreateTemporaryURLTable() {
  return CreateURLTable(true);
}

bool URLDatabase::CommitTemporaryURLTable() {
  // See the comments in the header file as well as
  // HistoryBackend::DeleteAllHistory() for more information on how this works
  // and why it does what it does.

  // Swap the url table out and replace it with the temporary one.
  if (!GetDB().Execute("DROP TABLE urls")) {
    DUMP_WILL_BE_NOTREACHED() << GetDB().GetErrorMessage();
    return false;
  }
  if (!GetDB().Execute("ALTER TABLE temp_urls RENAME TO urls")) {
    NOTREACHED_IN_MIGRATION() << GetDB().GetErrorMessage();
    return false;
  }

  // Re-create the index over the now permanent URLs table -- this was not there
  // for the temporary table.
  return CreateMainURLIndex();
}

bool URLDatabase::InitURLEnumeratorForEverything(URLEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  enumerator->statement_.Assign(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT " HISTORY_URL_ROW_FIELDS " FROM urls"));
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator->statement_.is_valid();
}

bool URLDatabase::InitURLEnumeratorForSignificant(URLEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  static constexpr char kSql[] =
      "SELECT" HISTORY_URL_ROW_FIELDS
      "FROM urls WHERE hidden = 0 AND "
      "(last_visit_time >= ? OR visit_count >= ? OR typed_count >= ?)"
      " ORDER BY typed_count DESC, last_visit_time DESC, visit_count DESC";
  enumerator->statement_.Assign(GetDB().GetUniqueStatement(kSql));
  enumerator->statement_.BindTime(0, AutocompleteAgeThreshold());
  enumerator->statement_.BindInt(1, kLowQualityMatchVisitLimit);
  enumerator->statement_.BindInt(2, kLowQualityMatchTypedLimit);
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator->statement_.is_valid();
}

bool URLDatabase::AutocompleteForPrefix(const std::string& prefix,
                                        size_t max_results,
                                        bool typed_only,
                                        URLRows* results) {
  // NOTE: this query originally sorted by starred as the second parameter. But
  // as bookmarks is no longer part of the db we no longer include the order
  // by clause.
  results->clear();

  sql::Statement statement;
  if (typed_only) {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT" HISTORY_URL_ROW_FIELDS
        "FROM urls "
        "WHERE url >= ? AND url < ? AND hidden = 0 AND typed_count > 0 "
        "ORDER BY typed_count DESC, visit_count DESC, last_visit_time DESC "
        "LIMIT ?"));
  } else {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT" HISTORY_URL_ROW_FIELDS
        "FROM urls "
        "WHERE url >= ? AND url < ? AND hidden = 0 "
        "ORDER BY typed_count DESC, visit_count DESC, last_visit_time DESC "
        "LIMIT ?"));
  }

  // We will find all strings between "prefix" and this string, which is prefix
  // followed by the maximum character size. Use 8-bit strings for everything
  // so we can be sure sqlite is comparing everything in 8-bit mode. Otherwise,
  // it will have to convert strings either to UTF-8 or UTF-16 for comparison.
  std::string end_query = database_utils::UpperBoundString(prefix);

  statement.BindString(0, prefix);
  statement.BindString(1, end_query);
  statement.BindInt(2, static_cast<int>(max_results));

  while (statement.Step()) {
    URLRow info;
    if (FillURLRow(statement, &info)) {
      results->push_back(info);
    }
  }
  return !results->empty();
}

bool URLDatabase::IsTypedHost(const std::string& host, std::string* scheme) {
  const char* schemes[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFtpScheme
  };
  URLRows dummy;
  for (const char* known_scheme : schemes) {
    std::string scheme_and_host(known_scheme);
    scheme_and_host += url::kStandardSchemeSeparator + host;
    if (AutocompleteForPrefix(scheme_and_host + '/', 1, true, &dummy) ||
        AutocompleteForPrefix(scheme_and_host + ':', 1, true, &dummy)) {
      if (scheme != nullptr)
        *scheme = known_scheme;
      return true;
    }
  }
  return false;
}

bool URLDatabase::FindShortestURLFromBase(const std::string& base,
                                          const std::string& url,
                                          int min_visits,
                                          int min_typed,
                                          bool allow_base,
                                          URLRow* info) {
  DCHECK(info);

  // Select URLs that start with `base` and are prefixes of `url`.  All parts
  // of this query except the substr() call can be done using the index.  We
  // could do this query with a couple of LIKE or GLOB statements as well, but
  // those wouldn't use the index, and would run into problems with "wildcard"
  // characters that appear in URLs (% for LIKE, or *, ? for GLOB).
  std::string sql =
      base::StrCat({"SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE url ",
                    allow_base ? ">=" : ">",
                    // Avoid limiting to 1 read to guard against the
                    // hypothetical case that a URL stored in the database is
                    // invalid, which requires moving on to the next best.
                    " ? AND url < :end AND url = substr(:end, 1, length(url)) "
                    "AND hidden = 0 AND visit_count >= ? AND typed_count >= ? "
                    "ORDER BY url"});
  sql::Statement statement(GetDB().GetUniqueStatement(sql));
  statement.BindString(0, base);
  statement.BindString(1, url);   // :end
  statement.BindInt(2, min_visits);
  statement.BindInt(3, min_typed);

  while (statement.Step()) {
    if (FillURLRow(statement, info)) {
      return true;
    }
  }

  return false;
}

URLRows URLDatabase::GetTextMatches(const std::u16string& query) {
  return GetTextMatchesWithAlgorithm(query,
                                     query_parser::MatchingAlgorithm::DEFAULT);
}

URLRows URLDatabase::GetTextMatchesWithAlgorithm(
    const std::u16string& query,
    query_parser::MatchingAlgorithm algorithm) {
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(query, algorithm, &query_nodes);

  URLRows results;
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE hidden = 0"));

  while (statement.Step()) {
    query_parser::QueryWordVector query_words;
    std::u16string url = base::i18n::ToLower(statement.ColumnString16(1));
    query_parser::QueryParser::ExtractQueryWords(url, &query_words);
    GURL gurl(url);
    if (gurl.is_valid()) {
      // Decode punycode to match IDN.
      std::u16string ascii = base::ASCIIToUTF16(gurl.host());
      std::u16string utf = url_formatter::IDNToUnicode(gurl.host());
      if (ascii != utf)
        query_parser::QueryParser::ExtractQueryWords(utf, &query_words);
    }
    std::u16string title = base::i18n::ToLower(statement.ColumnString16(2));
    query_parser::QueryParser::ExtractQueryWords(title, &query_words);

    if (query_parser::QueryParser::DoesQueryMatch(query_words, query_nodes)) {
      URLResult info;
      if (FillURLRow(statement, &info)) {
        results.push_back(info);
      }
    }
  }
  return results;
}

bool URLDatabase::InitKeywordSearchTermsTable() {
  has_keyword_search_terms_ = true;
  if (!GetDB().DoesTableExist("keyword_search_terms")) {
    if (!GetDB().Execute(
            "CREATE TABLE keyword_search_terms ("
            "keyword_id INTEGER NOT NULL,"  // ID of the TemplateURL.
            "url_id INTEGER NOT NULL,"      // ID of the url.
            "term LONGVARCHAR NOT NULL,"    // The actual search term.
            // The search term, in lower case, and with whitespaces collapsed.
            "normalized_term LONGVARCHAR NOT NULL)") ||
        !CreateKeywordSearchTermsIndices()) {
      return false;
    }
  }
  return true;
}

bool URLDatabase::CreateKeywordSearchTermsIndices() {
  // For searching.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index1 ON "
          "keyword_search_terms (keyword_id, normalized_term)")) {
    return false;
  }

  // For deletion.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index2 ON "
          "keyword_search_terms (url_id)")) {
    return false;
  }

  // For query or deletion by term.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index3 ON "
          "keyword_search_terms (term)")) {
    return false;
  }
  return true;
}

bool URLDatabase::DropKeywordSearchTermsTable() {
  // This will implicitly delete the indices over the table.
  return GetDB().Execute("DROP TABLE keyword_search_terms");
}

bool URLDatabase::SetKeywordSearchTermsForURL(URLID url_id,
                                              KeywordID keyword_id,
                                              const std::u16string& term) {
  DCHECK(url_id && keyword_id && !term.empty());

  sql::Statement exist_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT term FROM keyword_search_terms "
      "WHERE keyword_id = ? AND url_id = ?"));
  exist_statement.BindInt64(0, keyword_id);
  exist_statement.BindInt64(1, url_id);

  if (exist_statement.Step())
    return true;  // Term already exists, no need to add it.

  if (!exist_statement.Succeeded())
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO keyword_search_terms (keyword_id, url_id, term, "
      "normalized_term) VALUES (?,?,?,?)"));
  statement.BindInt64(0, keyword_id);
  statement.BindInt64(1, url_id);
  statement.BindString16(2, term);
  statement.BindString16(
      3, base::i18n::ToLower(base::CollapseWhitespace(term, false)));
  return statement.Run();
}

bool URLDatabase::GetAggregateURLDataForKeywordSearchTerm(
    const std::u16string& term,
    URLRow* url_info) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT SUM(u.visit_count), SUM(u.typed_count), MAX(u.last_visit_time) "
      "FROM keyword_search_terms kst JOIN urls u ON kst.url_id = u.id "
      "WHERE kst.normalized_term=? GROUP BY kst.normalized_term"));
  statement.BindString16(
      0, base::i18n::ToLower(base::CollapseWhitespace(term, false)));

  if (!statement.Step()) {
    return false;
  }

  if (url_info) {
    url_info->set_visit_count(statement.ColumnInt(0));
    url_info->set_typed_count(statement.ColumnInt(1));
    url_info->set_last_visit(statement.ColumnTime(2));
  }
  return true;
}

bool URLDatabase::GetKeywordSearchTermRow(URLID url_id,
                                          KeywordSearchTermRow* row) {
  DCHECK(url_id);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT keyword_id, term, normalized_term "
                                 "FROM keyword_search_terms WHERE url_id=?"));
  statement.BindInt64(0, url_id);

  if (!statement.Step())
    return false;

  if (row) {
    row->url_id = url_id;
    row->keyword_id = statement.ColumnInt64(0);
    row->term = statement.ColumnString16(1);
    row->normalized_term = statement.ColumnString16(2);
  }
  return true;
}

bool URLDatabase::GetKeywordSearchTermRows(
    const std::u16string& term,
    std::vector<KeywordSearchTermRow>* rows) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT keyword_id, url_id, normalized_term "
                                 "FROM keyword_search_terms WHERE term=?"));
  statement.BindString16(0, term);

  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    KeywordSearchTermRow row;
    row.url_id = statement.ColumnInt64(1);
    row.keyword_id = statement.ColumnInt64(0);
    row.term = term;
    row.normalized_term = statement.ColumnString16(2);
    rows->push_back(std::move(row));
  }
  return true;
}

void URLDatabase::DeleteAllSearchTermsForKeyword(
    KeywordID keyword_id) {
  DCHECK(keyword_id);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM keyword_search_terms WHERE keyword_id=?"));
  statement.BindInt64(0, keyword_id);

  statement.Run();
}

std::unique_ptr<KeywordSearchTermVisitEnumerator>
URLDatabase::CreateKeywordSearchTermVisitEnumerator(
    KeywordID keyword_id,
    const std::u16string& prefix) {
  // NOTE: the keyword_id can be zero if on first run the user does a query
  // before the TemplateURLService has finished loading. As the chances of this
  // occurring are small, we ignore it.
  if (!keyword_id)
    return nullptr;

  auto enumerator = base::WrapUnique<KeywordSearchTermVisitEnumerator>(
      new KeywordSearchTermVisitEnumerator());
  enumerator->statement_.Assign(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                           R"(
      SELECT
        kst.term,
        kst.normalized_term,
        u.visit_count,
        u.last_visit_time
      FROM
        keyword_search_terms kst JOIN urls u ON kst.url_id = u.id
      WHERE
        kst.keyword_id = ? AND
        kst.normalized_term >= ? AND
        kst.normalized_term < ?
      ORDER BY kst.normalized_term, u.last_visit_time
      )"));
  // Keep CollapseWhitespace() and ToLower() in sync with search_provider.cc.
  std::u16string normalized_prefix =
      base::CollapseWhitespace(base::i18n::ToLower(prefix), false);
  // This magic gives us a prefix search.
  std::u16string next_prefix = normalized_prefix;
  next_prefix.back() = next_prefix.back() + 1;
  enumerator->statement_.BindInt64(0, keyword_id);
  enumerator->statement_.BindString16(1, normalized_prefix);
  enumerator->statement_.BindString16(2, next_prefix);
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator;
}

std::unique_ptr<KeywordSearchTermVisitEnumerator>
URLDatabase::CreateKeywordSearchTermVisitEnumerator(KeywordID keyword_id) {
  // NOTE: the keyword_id can be zero if on first run the user does a query
  // before the TemplateURLService has finished loading. As the chances of this
  // occurring are small, we ignore it.
  if (!keyword_id)
    return nullptr;

  auto enumerator = base::WrapUnique<KeywordSearchTermVisitEnumerator>(
      new KeywordSearchTermVisitEnumerator());
  enumerator->statement_.Assign(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                           R"(
      SELECT
        kst.term,
        kst.normalized_term,
        u.visit_count,
        u.last_visit_time
      FROM
        keyword_search_terms kst JOIN urls u ON kst.url_id = u.id
      WHERE
        kst.keyword_id = ? AND
        kst.normalized_term <> ''
      ORDER BY kst.normalized_term, u.last_visit_time
      )"));
  enumerator->statement_.BindInt64(0, keyword_id);
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator;
}

bool URLDatabase::DeleteKeywordSearchTerm(const std::u16string& term) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM keyword_search_terms WHERE term=?"));
  statement.BindString16(0, term);

  return statement.Run();
}

bool URLDatabase::DeleteKeywordSearchTermForNormalizedTerm(
    KeywordID keyword_id,
    const std::u16string& normalized_term) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "DELETE FROM keyword_search_terms WHERE "
                                 "keyword_id = ? AND normalized_term=?"));
  statement.BindInt64(0, keyword_id);
  statement.BindString16(1, normalized_term);

  return statement.Run();
}

bool URLDatabase::DeleteKeywordSearchTermForURL(URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM keyword_search_terms WHERE url_id=?"));
  statement.BindInt64(0, url_id);
  return statement.Run();
}

bool URLDatabase::DropStarredIDFromURLs() {
  if (!GetDB().DoesColumnExist("urls", "starred_id"))
    return true;  // urls is already updated, no need to continue.

  return RecreateURLTableWithAllContents();
}

bool URLDatabase::CreateURLTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_urls" : "urls";
  if (GetDB().DoesTableExist(name)) {
    if (!is_temporary) {
      return true;
    }
    if (!GetDB().Execute("DROP TABLE temp_urls")) {
      NOTREACHED_IN_MIGRATION() << GetDB().GetErrorMessage();
      return false;
    }
  }

  // Note: revise implementation for InsertOrUpdateURLRowByID() if you add any
  // new constraints to the schema.
  std::string sql = base::StrCat(
      {"CREATE TABLE ", name,
       // The id uses AUTOINCREMENT is for sync propose. Sync uses this `id` as
       // an unique key to identify the URLs. If here did not use AUTOINCREMENT,
       // and Sync was not working somehow, a ROWID could be deleted and re-used
       // during this period. Once Sync come back, Sync would use ROWIDs and
       // timestamps to see if there are any updates need to be synced. And sync
       // will only see the new URL, but missed the deleted URL.
       "(id INTEGER PRIMARY KEY AUTOINCREMENT,"
       "url LONGVARCHAR,"
       "title LONGVARCHAR,"
       "visit_count INTEGER DEFAULT 0 NOT NULL,"
       "typed_count INTEGER DEFAULT 0 NOT NULL,"
       "last_visit_time INTEGER NOT NULL,"
       "hidden INTEGER DEFAULT 0 NOT NULL)"});
  // IMPORTANT: If you change the columns, also update in_memory_database.cc
  // where the values are copied (InitFromDisk).

  return GetDB().Execute(sql);
}

bool URLDatabase::CreateMainURLIndex() {
  return GetDB().Execute(
      "CREATE INDEX IF NOT EXISTS urls_url_index ON urls (url)");
}

bool URLDatabase::RecreateURLTableWithAllContents() {
  // Create a temporary table to contain the new URLs table.
  if (!CreateTemporaryURLTable()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // Copy the contents.
  if (!GetDB().Execute(
          "INSERT INTO temp_urls (id, url, title, visit_count, typed_count, "
          "last_visit_time, hidden) "
          "SELECT id, url, title, visit_count, typed_count, last_visit_time, "
          "hidden FROM urls")) {
    const char* error_message = GetDB().GetErrorMessage();
    if (error_message) {
      // TODO(crbug.com/40901889): used in understanding why this is happening.
      // Remove once bug is fixed.
      static crash_reporter::CrashKeyString<256> error_message_crash_key(
          "recreate_url_table_description");
      error_message_crash_key.Set(error_message);
    }
    DUMP_WILL_BE_NOTREACHED() << error_message;
    return false;
  }

  // Rename/commit the tmp table.
  return CommitTemporaryURLTable();
}

const int kLowQualityMatchTypedLimit = 1;
const int kLowQualityMatchVisitLimit = 4;
const int kLowQualityMatchAgeLimitInDays = 3;

base::Time AutocompleteAgeThreshold() {
  return (base::Time::Now() - base::Days(kLowQualityMatchAgeLimitInDays));
}

bool RowQualifiesAsSignificant(const URLRow& row,
                               const base::Time& threshold) {
  if (row.hidden())
    return false;

  const base::Time& real_threshold =
      threshold.is_null() ? AutocompleteAgeThreshold() : threshold;
  return (row.typed_count() >= kLowQualityMatchTypedLimit) ||
         (row.visit_count() >= kLowQualityMatchVisitLimit) ||
         (row.last_visit() >= real_threshold);
}

}  // namespace history
