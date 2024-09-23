// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_DATABASE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_DATABASE_H_

#include <memory>
#include <vector>

#include "components/affiliations/core/browser/affiliation_utils.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace affiliations {

// Stores equivalence classes of facets, i.e., facets that are affiliated with
// each other, in an SQLite database. In addition, relevant branding information
// is stored. See affiliation_utils.h for a more detailed definition of what
// this means.
//
// Under the assumption that there is most likely not much the caller can do in
// case of database errors, most methods silently ignore them. Nevertheless, the
// caller must plan ahead for this rare but non-negligible scenario, and expect
// that in odd cases basic database invariants will not hold.
class AffiliationDatabase {
 public:
  AffiliationDatabase();

  AffiliationDatabase(const AffiliationDatabase&) = delete;
  AffiliationDatabase& operator=(const AffiliationDatabase&) = delete;

  ~AffiliationDatabase();

  // Opens an existing database at |path|, or creates a new one if none exists,
  // and returns true on success.
  bool Init(const base::FilePath& path);

  // Looks up the equivalence class containing |facet_uri|, and returns true if
  // such a class is found, in which case it is also stored into |result|
  // together with branding information, if applicable.
  bool GetAffiliationsAndBrandingForFacetURI(
      const FacetURI& facet_uri,
      AffiliatedFacetsWithUpdateTime* result) const;

  // Retrieves all stored equivalence classes and branding information.
  void GetAllAffiliationsAndBranding(
      std::vector<AffiliatedFacetsWithUpdateTime>* results) const;

  // Retrieves all stored groups.
  std::vector<GroupedFacets> GetAllGroups() const;

  // Retrieves a group for |facet_uri| or empty group with only |facet_uri| if
  // there are no matches in the database.
  GroupedFacets GetGroup(const FacetURI& facet_uri) const;

  // Retrieves psl extension list.
  std::vector<std::string> GetPSLExtensions() const;

  // Removes the stored equivalence class and branding information, if any,
  // containing |facet_uri|.
  void DeleteAffiliationsAndBrandingForFacetURI(const FacetURI& facet_uri);

  // Stores the equivalence class and branding information defined by
  // |affiliated_facets| to the database, and removes any other equivalence
  // classes that are in conflict with |affiliated_facets|, i.e. those that are
  // neither equal nor disjoint to it. Removed equivalence classes are stored
  // into |removed_affiliations|.
  void StoreAndRemoveConflicting(
      const AffiliatedFacetsWithUpdateTime& affiliated_facets,
      const GroupedFacets& group,
      std::vector<AffiliatedFacetsWithUpdateTime>* removed_affiliations);

  // Removes all the stored equivalence classes and branding information which
  // aren't represented by |facet_uris|.
  void RemoveMissingFacetURI(std::vector<FacetURI> facet_uris);

  // Deletes the database file at |path| along with all its auxiliary files. The
  // database must be closed before calling this.
  static void Delete(const base::FilePath& path);

  // Exposes the version of the underlying database. Should only be used in
  // tests.
  int GetDatabaseVersionForTesting();

  // Updates |psl_extensions| table with provided |domains|.
  void UpdatePslExtensions(const std::vector<std::string>& domains);

 private:
  // Represents possible results of AffiliationDatabase::Store call.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Always keep this enum in sync with
  // the corresponding PasswordCheckInteraction in enums.xml
  enum class StoreAffiliationResult {
    kSuccess = 0,
    kFailedToStartTransaction = 1,
    kFailedToCloseTransaction = 2,
    kFailedToAddSet = 3,
    kFailedToAddAffiliation = 4,
    kFailedToAddGroup = 5,
    // Must be last.
    kMaxValue = kFailedToAddGroup,
  };

  // Stores the equivalence class and branding information defined by
  // |affiliated_facets| to the DB and returns true unless it has a non-empty
  // subset with a preexisting class, in which case no changes are made and the
  // function returns false.
  StoreAffiliationResult Store(
      const AffiliatedFacetsWithUpdateTime& affiliated_facets,
      const GroupedFacets& group);

  // Called when SQLite encounters an error.
  void SQLErrorCallback(int error_number, sql::Statement* statement);

  // The SQL connection to the database.
  std::unique_ptr<sql::Database> sql_connection_;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_DATABASE_H_
