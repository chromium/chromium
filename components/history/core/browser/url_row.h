// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
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

  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) {
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

    bool operator()(const URLRow& row) { return row.url() == (*url_); }

   private:
    const raw_ref<const GURL> url_;
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

  std::u16string title_;

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

  // We support the implicit copy constructor and operator=.
};
typedef std::vector<URLRow> URLRows;

// Annotations -----------------------------------------------------------------

// A set of binary state related to a page visit. To be used for bit masking
// operations.
//
// These values are persisted in database. Entries should not be renumbered and
// numeric values should never be reused.
enum VisitContentAnnotationFlag : uint64_t {
  kNone = 0,

  // No longer used in production code. Only referenced in a database migration
  // test.
  kDeprecatedFlocEligibleRelaxed = 1ULL << 0,

  // Indicates that the annotated page can be included in browsing topics
  // calculation (https://github.com/jkarlin/topics). A page visit is eligible
  // for browsing topics calculation if all of the conditions hold:
  // 1. The IP of this visit is publicly routable, i.e. the IP is NOT within
  // the ranges reserved for "private" internet
  // (https://tools.ietf.org/html/rfc1918).
  // 2. The browsing-topics Permissions Policy feature is allowed in the page.
  // 3. Page opted in: document.browsingTopics() API is used in the page.
  kBrowsingTopicsEligible = 1ULL << 1,
};

using VisitContentAnnotationFlags = uint64_t;

// A structure containing annotations computed by ML models to page content
// for a visit. Be cautious when changing the default values as they may already
// have been written to the storage.
struct VisitContentModelAnnotations {
  static constexpr float kDefaultVisibilityScore = -1;
  static constexpr int kDefaultPageTopicsModelVersion = -1;

  struct Category {
    Category();
    Category(const std::string& id, int weight);
    // |vector| is expected to be of size 2 with the first entry being an ID of
    // string or int type and the second entry indicating an integer weight.
    static std::optional<Category> FromStringVector(
        const std::vector<std::string>& vector);
    std::string ToString() const;
    bool operator==(const Category& other) const;
    bool operator!=(const Category& other) const;

    std::string id;
    int weight = 0;
  };

  VisitContentModelAnnotations();
  VisitContentModelAnnotations(float visibility_score,
                               const std::vector<Category>& categories,
                               int64_t page_topics_model_version,
                               const std::vector<Category>& entities);
  VisitContentModelAnnotations(const VisitContentModelAnnotations& other);
  ~VisitContentModelAnnotations();

  // Merges `category` into `categories`. It upgrades the weight if it already
  // exists, and appends it if it doesn't.
  static void MergeCategoryIntoVector(const Category& category,
                                      std::vector<Category>* categories);

  // Merges the max-score, categories, and entities from `other`, which is the
  // content model annotations of a duplicate visit.
  void MergeFrom(const VisitContentModelAnnotations& other);

  // A value from 0 to 1 that represents how prominent, or visible, the page
  // might be considered on UI surfaces.
  float visibility_score = kDefaultVisibilityScore;
  // A vector that contains category IDs and their weights. It is guaranteed
  // that there will not be duplicates in the category IDs contained in this
  // field.
  std::vector<Category> categories;
  // The version of the page topics model that was used to annotate content.
  int64_t page_topics_model_version = kDefaultPageTopicsModelVersion;
  // A vector that contains entity IDs and their weights. It is guaranteed
  // that there will not be duplicates in the category IDs contained in this
  // field.
  std::vector<Category> entities;

  // Any field added here must also update the
  // `MergeUpdateIntoExistingModelAnnotations` function in history_backend.cc.
};

// A structure containing the annotations made to page content for a visit.
//
// Note: only `page_language`, `password_state`, `has_url_keyed_image`,
// `related_searches` and `model_annotations.categories` are being synced to
// remote devices; other fields should not be synced without auditing the usages
// ( e.g. `BrowsingTopicsCalculator` is currently assuming that a visit entry
// comes from the local history as long as it is associated with a non-empty
// `annotation_flags`).
struct VisitContentAnnotations {
  // Values are persisted; do not reorder or reuse, and only add new values at
  // the end.
  enum class PasswordState {
    kUnknown = 0,
    kNoPasswordField = 1,
    kHasPasswordField = 2,
  };

  VisitContentAnnotations();
  VisitContentAnnotations(VisitContentAnnotationFlags annotation_flags,
                          VisitContentModelAnnotations model_annotations,
                          const std::vector<std::string>& related_searches,
                          const GURL& search_normalized_url,
                          const std::u16string& search_terms,
                          const std::string& alternative_title,
                          const std::string& page_language,
                          PasswordState password_state,
                          bool has_url_keyed_image);
  VisitContentAnnotations(const VisitContentAnnotations& other);
  ~VisitContentAnnotations();

  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kNone;
  VisitContentModelAnnotations model_annotations;
  // A vector that contains related searches for a Google SRP visit.
  std::vector<std::string> related_searches;
  GURL search_normalized_url;
  std::u16string search_terms;
  // Alternative page title for the visit.
  std::string alternative_title;
  // Language of the content on the page, as an ISO 639 language code (usually
  // two letters). May be "und" if the language couldn't be determined.
  std::string page_language;
  // Whether a password form was found on the page - see also
  // sessions::SerializedNavigationEntry::PasswordState.
  PasswordState password_state = PasswordState::kUnknown;
  // Whether there is a URL-keyed image for this visit.
  bool has_url_keyed_image = false;
};

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

  const VisitContentAnnotations& content_annotations() const {
    return content_annotations_;
  }
  void set_content_annotations(
      const VisitContentAnnotations& content_annotations) {
    content_annotations_ = content_annotations;
  }

  const query_parser::Snippet& snippet() const { return snippet_; }

  bool blocked_visit() const { return blocked_visit_; }
  void set_blocked_visit(bool blocked_visit) {
    blocked_visit_ = blocked_visit;
  }

  std::optional<std::string> app_id() const { return app_id_; }
  void set_app_id(std::optional<std::string> app_id) { app_id_ = app_id; }

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

  // The annotations made to the page content for this visit.
  VisitContentAnnotations content_annotations_;

  // These values are typically set by HistoryBackend.
  query_parser::Snippet snippet_;
  query_parser::Snippet::MatchPositions title_match_positions_;

  // Whether a managed user was blocked when attempting to visit this URL.
  bool blocked_visit_ = false;

  // ID of the app this entry was generated for. Set to a non-null value
  // on Android only.
  std::optional<std::string> app_id_;

  // We support the implicit copy constructor and operator=.
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_URL_ROW_H_
