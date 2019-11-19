// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_

#include <stdint.h>

#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/query_parser/snippet.h"
#include "url/gurl.h"

namespace history {

typedef int64_t URLID;

// Holds all information globally associated with one URL (one row in the
// URL table).
class URLRow {
 public:
  URLRow();

  explicit URLRow(const GURL& url);

  // We need to be able to set the id of a URLRow that's being passed through
  // an IPC message.  This constructor should probably not be used otherwise.
  URLRow(const GURL& url, URLID id);

  URLRow(const URLRow& other);
  URLRow(URLRow&&) noexcept;

  virtual ~URLRow();
  URLRow& operator=(const URLRow& other);
  URLRow& operator=(URLRow&& other) noexcept;

  URLID id() const { return id_; }

  // Sets the id of the row. The id should only be manually set when a row has
  // been retrieved from the history database or other dataset based on criteria
  // other than its id (i.e. by URL) and when the id has not yet been set in the
  // row.
  void set_id(URLID id) { id_ = id; }

  void set_url(const GURL& url) { url_ = url; }
  const GURL& url() const { return url_; }

  const base::string16& title() const {
    return title_;
  }
  void set_title(const base::string16& title) {
    // The title is frequently set to the same thing, so we don't bother
    // updating unless the string has changed.
    if (title != title_) {
      title_ = title;
    }
  }

  // The number of times this URL has been visited. This will often match the
  // number of entries in the visit table for this URL, but won't always. It's
  // really designed for autocomplete ranking, so some "useless" transitions
  // from the visit table aren't counted in this tally.
  int visit_count() const {
    return visit_count_;
  }
  void set_visit_count(int visit_count) {
    visit_count_ = visit_count;
  }

  // Number of times the URL was typed in the Omnibox. This "should" match
  // the number of TYPED transitions in the visit table. It's used primarily
  // for faster autocomplete ranking. If you need to know the actual number of
  // TYPED transitions, you should query the visit table since there could be
  // something out of sync.
  int typed_count() const {
    return typed_count_;
  }
  void set_typed_count(int typed_count) {
    typed_count_ = typed_count;
  }

  base::Time last_visit() const {
    return last_visit_;
  }
  void set_last_visit(base::Time last_visit) {
    last_visit_ = last_visit;
  }

  // If this is set, we won't autocomplete this URL.
  bool hidden() const {
    return hidden_;
  }
  void set_hidden(bool hidden) {
    hidden_ = hidden;
  }

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Helper functor that determines if an URLRow refers to a given URL.
  class URLRowHasURL {
   public:
    explicit URLRowHasURL(const GURL& url) : url_(url) {}

    bool operator()(const URLRow& row) {
      return row.url() == url_;
    }

   private:
    const GURL& url_;
  };

 protected:
  // Swaps the contents of this URLRow with another, which allows it to be
  // destructively copied without memory allocations.
  void Swap(URLRow* other);

 private:
  // The row ID of this URL from the history database. This is immutable except
  // when retrieving the row from the database or when determining if the URL
  // referenced by the URLRow already exists in the database.
  URLID id_ = 0;

  // The URL of this row. Immutable except for the database which sets it
  // when it pulls them out. If clients want to change it, they must use
  // the constructor to make a new one.
  GURL url_;

  base::string16 title_;

  // Total number of times this URL has been visited.
  int visit_count_ = 0;

  // Number of times this URL has been manually entered in the URL bar.
  int typed_count_ = 0;

  // The date of the last visit of this URL, which saves us from having to
  // loop up in the visit table for things like autocomplete and expiration.
  base::Time last_visit_;

  // Indicates this entry should now be shown in typical UI or queries, this
  // is usually for subframes.
  bool hidden_ = false;

  // We support the implicit copy constuctor and operator=.
};
typedef std::vector<URLRow> URLRows;


class URLResult : public URLRow {
 public:
  URLResult();
  URLResult(const GURL& url, base::Time visit_time);
  URLResult(const URLRow& url_row);
  URLResult(const URLResult& other);
  URLResult(URLResult&&) noexcept;
  ~URLResult() override;

  URLResult& operator=(const URLResult&);

  base::Time visit_time() const { return visit_time_; }
  void set_visit_time(base::Time visit_time) { visit_time_ = visit_time; }

  const query_parser::Snippet& snippet() const { return snippet_; }

  bool blocked_visit() const { return blocked_visit_; }
  void set_blocked_visit(bool blocked_visit) {
    blocked_visit_ = blocked_visit;
  }

  // If this is a title match, title_match_positions contains an entry for
  // every word in the title that matched one of the query parameters. Each
  // entry contains the start and end of the match.
  const query_parser::Snippet::MatchPositions& title_match_positions() const {
    return title_match_positions_;
  }

  void SwapResult(URLResult* other);

  static bool CompareVisitTime(const URLResult& lhs, const URLResult& rhs);

 private:
  friend class HistoryBackend;

  // The time that this result corresponds to.
  base::Time visit_time_;

  // These values are typically set by HistoryBackend.
  query_parser::Snippet snippet_;
  query_parser::Snippet::MatchPositions title_match_positions_;

  // Whether a managed user was blocked when attempting to visit this URL.
  bool blocked_visit_ = false;

  // We support the implicit copy constructor and operator=.
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_
