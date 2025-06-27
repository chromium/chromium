// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_URL_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_URL_DATABASE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_row.h"
#include "components/query_parser/query_parser.h"
#include "sql/statement.h"

class GURL;

namespace base {
class Time;
}

namespace sql {
class Database;
}

namespace history {

class KeywordSearchTermVisitEnumerator;
struct KeywordSearchTermRow;

class VisitDatabase;  // For friend statement.

// Encapsulates an SQL database that holds URL info.  This is a subset of the
// full history data.  We split this class' functionality out from the larger
// HistoryDatabase class to support maintaining separate databases of URLs with
// different capabilities (for example, the in-memory database).
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class URLDatabase {
 public:
  // Must call CreateURLTable() and CreateURLIndexes() before using to make
  // sure the database is initialized.
  URLDatabase();

  URLDatabase(const URLDatabase&) = delete;
  URLDatabase& operator=(const URLDatabase&) = delete;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~URLDatabase();

  // URL table functions -------------------------------------------------------

  // Looks up a url given an id. Fills info with the data. Returns true on
  // success and false otherwise.
  bool GetURLRow(URLID url_id, URLRow* info);

  // Looks up the given URL and if it exists, fills the given pointers with the
  // associated info and returns the ID of that URL. If the info pointer is
  // NULL, no information about the URL will be filled in, only the ID will be
  // returned. Returns 0 if the URL was not found.
  URLID GetRowForURL(const GURL& url, URLRow* info);

  // Given an already-existing row in the URL table, updates that URL's stats.
  // This can not change the URL.  Returns true on success.
  //
  // This will NOT update the title used for full text indexing. If you are
  // setting the title, call SetPageIndexedData with the new title.
  bool UpdateURLRow(URLID url_id, const URLRow& info);

  // Adds a line to the URL database with the given information and returns the
  // newly generated ID for the row (the `id` in `info` is ignored). A row with
  // the given URL must not exist. Returns 0 on error.
  //
  // This does NOT add a row to the full text search database. Use
  // HistoryDatabase::SetPageIndexedData to do this.
  URLID AddURL(const URLRow& info) {
    return AddURLInternal(info, false);
  }

  // Either adds a new row to the URL table with the given information (with the
  // the `id` as specified in `info`), or updates the pre-existing row with this
  // `id` if there is one already. This is also known as an "upsert" or "merge"
  // operation. Returns true on success.
  bool InsertOrUpdateURLRowByID(const URLRow& info);

  // Delete the row of the corresponding URL. Only the row in the URL table and
  // corresponding keyword search terms will be deleted, not any other data that
  // may refer to the URL row. Returns true if the row existed and was deleted.
  bool DeleteURLRow(URLID id);

  // URL mass-deleting ---------------------------------------------------------

  // Begins the mass-deleting operation by creating a temporary URL table.
  // The caller than adds the URLs it wants to preserve to the temporary table,
  // and then deletes everything else by calling CommitTemporaryURLTable().
  // Returns true on success.
  //
  // WARNING: if the temporary table already exists, it is dropped and a new
  // one created. This is done as the temporary table is only intended to
  // exist for a short amount of time before it's renamed.
  bool CreateTemporaryURLTable();

  // Adds a row to the temporary URL table. This must be called between
  // CreateTemporaryURLTable() and CommitTemporaryURLTable() (see those for more
  // info). The ID of the URL will change in the temporary table, so the new ID
  // is returned. Returns 0 on failure.
  URLID AddTemporaryURL(const URLRow& row) {
    return AddURLInternal(row, true);
  }

  // Ends the mass-deleting by replacing the original URL table with the
  // temporary one created in CreateTemporaryURLTable. Returns true on success.
  bool CommitTemporaryURLTable();

  // Enumeration ---------------------------------------------------------------

  // A basic enumerator to enumerate urls database.
  class URLEnumeratorBase {
   public:
    URLEnumeratorBase();

    URLEnumeratorBase(const URLEnumeratorBase&) = delete;
    URLEnumeratorBase& operator=(const URLEnumeratorBase&) = delete;

    virtual ~URLEnumeratorBase();

   private:
    friend class URLDatabase;

    bool initialized_;
    sql::Statement statement_;
  };

  // A basic enumerator to enumerate urls
  class URLEnumerator : public URLEnumeratorBase {
   public:
    URLEnumerator();

    URLEnumerator(const URLEnumerator&) = delete;
    URLEnumerator& operator=(const URLEnumerator&) = delete;

    // Retrieves the next url. Returns false if no more urls are available.
    bool GetNextURL(URLRow* r);
  };

  // Initializes the given enumerator to enumerate all URLs in the database.
  bool InitURLEnumeratorForEverything(URLEnumerator* enumerator);

  // Initializes the given enumerator to enumerate all URLs in the database that
  // are historically significant: ones having their URL manually typed at least
  // once, having been visited within 3 days, or having been visited at least 4
  // times in the order of the most significant ones first.
  bool InitURLEnumeratorForSignificant(URLEnumerator* enumerator);

  // Autocomplete --------------------------------------------------------------

  // Fills the given array with URLs matching the given prefix.  They will be
  // sorted by typed count, then by visit count, then by visit date (most recent
  // first) up to the given maximum number.  If `typed_only` is true, only urls
  // that have been typed once are returned.  For caller convenience, returns
  // whether any results were found.
  bool AutocompleteForPrefix(const std::string& prefix,
                             size_t max_results,
                             bool typed_only,
                             URLRows* results);

  // Returns true if the database holds some past typed navigation to a URL on
  // the provided hostname. If the return value is true and `scheme` is not
  // nullptr, `scheme` holds the scheme of one of the corresponding entries in
  // the database.
  bool IsTypedHost(const std::string& host, std::string* scheme);

  // Tries to find the shortest URL beginning with `base` that strictly
  // prefixes `url`, and has minimum visit_ and typed_counts as specified.
  // If found, fills in `info` and returns true; otherwise returns false,
  // leaving `info` unchanged.
  // We allow matches of exactly `base` iff `allow_base` is true.
  bool FindShortestURLFromBase(const std::string& base,
                               const std::string& url,
                               int min_visits,
                               int min_typed,
                               bool allow_base,
                               URLRow* info);

  // History search ------------------------------------------------------------

  // Performs a brute force search over the database to find any URLs or titles
  // which match the `query` string, using the default text matching algorithm.
  // Returns any matches.
  URLRows GetTextMatches(const std::u16string& query);

  // Same as GetTextMatches, using `algorithm` as the text matching
  // algorithm.
  URLRows GetTextMatchesWithAlgorithm(
      const std::u16string& query,
      query_parser::MatchingAlgorithm algorithm);

  // Keyword Search Terms ------------------------------------------------------

  // Sets the search terms for the specified url/keyword pair.
  bool SetKeywordSearchTermsForURL(URLID url_id,
                                   KeywordID keyword_id,
                                   const std::u16string& term);

  // Retrieves aggregate values for a subset of fields across all URLs
  // associated with the given `term`.
  // Fills `url_info` with the relevant aggregate URL data.
  // Returns true on success.
  bool GetAggregateURLDataForKeywordSearchTerm(const std::u16string& term,
                                               URLRow* url_info);

  // Looks up a keyword search term given a url id. Returns all the search terms
  // in `rows`. Returns true on success.
  bool GetKeywordSearchTermRow(URLID url_id, KeywordSearchTermRow* row);

  // Looks up all keyword search terms given a term, Fills the rows with data.
  // Returns true on success and false otherwise.
  bool GetKeywordSearchTermRows(const std::u16string& term,
                                std::vector<KeywordSearchTermRow>* rows);

  // Deletes all search terms for the specified keyword that have been added by
  // way of SetKeywordSearchTermsForURL.
  void DeleteAllSearchTermsForKeyword(KeywordID keyword_id);

  // Returns an enumerator to enumerate all the KeywordSearchTermVisits starting
  // with `prefix` for the given keyword. The visits are ordered first by
  // |normalized_term| and then by |last_visit_time| in ascending order, i.e.,
  // from the oldest to the newest.
  std::unique_ptr<KeywordSearchTermVisitEnumerator>
  CreateKeywordSearchTermVisitEnumerator(KeywordID keyword_id,
                                         const std::u16string& prefix);

  // Returns an enumerator to enumerate all the KeywordSearchTermVisits for the
  // given keyword. The visits are ordered first by |normalized_term| and then
  // by |last_visit_time| in ascending order, i.e.,from the oldest to the
  // newest.
  std::unique_ptr<KeywordSearchTermVisitEnumerator>
  CreateKeywordSearchTermVisitEnumerator(KeywordID keyword_id);

  // Deletes all searches matching `term`.
  bool DeleteKeywordSearchTerm(const std::u16string& term);

  // Deletes any search corresponding to `normalized_term`.
  bool DeleteKeywordSearchTermForNormalizedTerm(
      KeywordID keyword_id,
      const std::u16string& normalized_term);

  // Deletes any search corresponding to `url_id`.
  bool DeleteKeywordSearchTermForURL(URLID url_id);

 protected:
  friend class VisitDatabase;

  // See HISTORY_URL_ROW_FIELDS below.
  static const char kURLRowFields[];

  // The number of fiends in kURLRowFields. If callers need additional
  // fields, they can add their 0-based index to this value to get the index of
  // fields following kURLRowFields.
  static const int kNumURLRowFields;

  // Drops the starred_id column from urls, returning true on success. This does
  // nothing (and returns true) if the urls doesn't contain the starred_id
  // column.
  bool DropStarredIDFromURLs();

  // Initialization functions. The indexing functions are separate from the
  // table creation functions so the in-memory database and the temporary tables
  // used when clearing history can populate the table and then create the
  // index, which is faster than the reverse.
  //
  // is_temporary is false when generating the "regular" URLs table. The expirer
  // sets this to true to generate the temporary table, which will have a
  // different name but the same schema. See comment in
  // CreateTemporaryURLTable() for details on temporary creation.
  bool CreateURLTable(bool is_temporary);

  // Creates the index over URLs so we can quickly look up based on URL.
  bool CreateMainURLIndex();

  // Recreate URL table, and keep all existing contents.
  bool RecreateURLTableWithAllContents();

  // Ensures the keyword search terms table exists.
  bool InitKeywordSearchTermsTable();

  // Creates the indices used for keyword search terms.
  bool CreateKeywordSearchTermsIndices();

  // Deletes the keyword search terms table.
  bool DropKeywordSearchTermsTable();

  // Inserts the given URL row into the URLs table, using the regular table
  // if is_temporary is false, or the temporary URL table if is temporary is
  // true. The current `id` of `info` will be ignored in both cases and a new ID
  // will be generated, which will also constitute the return value, except in
  // case of an error, when the return value is 0. The temporary table may only
  // be used in between CreateTemporaryURLTable() and CommitTemporaryURLTable().
  URLID AddURLInternal(const URLRow& info, bool is_temporary);

  // Return true if the urls table's schema contains "AUTOINCREMENT".
  // false if table do not contain AUTOINCREMENT, or the table is not created.
  bool URLTableContainsAutoincrement();

  // Convenience to fill a URLRow. Must be in sync with the fields in
  // kHistoryURLRowFields. Returns true if the data was valid and |*i| was
  // actually populated.
  [[nodiscard]] static bool FillURLRow(sql::Statement& s, URLRow* i);

  // Returns the database for the functions in this interface. The descendant of
  // this class implements these functions to return its objects.
  virtual sql::Database& GetDB() = 0;

  // Replaces the lower_term column in the keyword search terms table with
  // normalized_term which contains the search term, in lower case, and with
  // whitespaces collapsed for migration to version 42.
  bool MigrateKeywordsSearchTermsLowerTermColumn();

 private:
  // True if InitKeywordSearchTermsTable() has been invoked. Not all subclasses
  // have keyword search terms.
  bool has_keyword_search_terms_;
};

// The fields and order expected by FillURLRow(). ID is guaranteed to be first
// so that DISTINCT can be prepended to get distinct URLs.
//
// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define HISTORY_URL_ROW_FIELDS \
    " urls.id, urls.url, urls.title, urls.visit_count, urls.typed_count, " \
    "urls.last_visit_time, urls.hidden "

// Constants which specify, when considered altogether, 'significant'
// history items. These are used to filter out insignificant items
// for consideration as autocomplete candidates.
extern const int kLowQualityMatchTypedLimit;
extern const int kLowQualityMatchVisitLimit;
extern const int kLowQualityMatchAgeLimitInDays;

// Returns the date threshold for considering an history item as significant.
base::Time AutocompleteAgeThreshold();

// Return true if `row` qualifies as an autocomplete candidate. If `threshold`
// is_null() then this function determines a new time threshold each time it is
// called. Since getting system time can be costly (such as for cases where
// this function will be called in a loop over many history items), you can
// provide a non-null `threshold` by simply initializing `threshold` with
// AutocompleteAgeThreshold() (or any other desired time in the past).
bool RowQualifiesAsSignificant(const URLRow& row, const base::Time& threshold);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_URL_DATABASE_H_
