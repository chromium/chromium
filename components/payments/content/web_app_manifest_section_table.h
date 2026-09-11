// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_WEB_APP_MANIFEST_SECTION_TABLE_H_
#define COMPONENTS_PAYMENTS_CONTENT_WEB_APP_MANIFEST_SECTION_TABLE_H_

#include <string>
#include <vector>

#include "components/payments/content/web_app_manifest.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace payments {

// This class manages the `web_app_manifest_section` SQLite table.
// The interfaces should only be accessed on the DB thread.
//
// This table caches native Android app verification requirements (min_version,
// certificate fingerprints) declared in the `related_applications` sections of
// Web App Manifests referenced as `default_applications` by a Payment Method
// Manifest (identified by `method_name`). For a given payment method, the table
// stores all sections across all of its default applications.
//
//  expire_date    The data expiry date in seconds from 1601-01-01 00:00:00 UTC.
//  method_name    The payment method identifier (acts as partition key).
//  id             The package name of the Android app. Rows are queried by
//                 the composite key (method_name, id).
//  min_version    Minimum version number of the app.
//  fingerprints   SHA256 fingerprints of signing certificate bytes.
//
class WebAppManifestSectionTable : public WebDatabaseTable {
 public:
  WebAppManifestSectionTable();

  WebAppManifestSectionTable(const WebAppManifestSectionTable&) = delete;
  WebAppManifestSectionTable& operator=(const WebAppManifestSectionTable&) =
      delete;

  ~WebAppManifestSectionTable() override;

  // Retrieves the WebAppManifestSectionTable* owned by |db|.
  static WebAppManifestSectionTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Remove expired data.
  void RemoveExpiredData();

  // Adds (or replaces) the cached web app manifest sections for
  // `payment_method` with `manifest`. Note that `manifest` should contain all
  // sections across all `default_applications` referenced by that payment
  // method; any previous sections for `payment_method` are deleted atomically.
  // `manifest` must not be empty.
  bool AddWebAppManifest(const std::string& payment_method,
                         const std::vector<WebAppManifestSection>& manifest);

  // Gets all cached verification sections for the Android package `web_app`
  // under `payment_method`. Returns an empty vector if no match is found.
  std::vector<WebAppManifestSection> GetWebAppManifest(
      const std::string& payment_method,
      const std::string& web_app);

 private:
  bool MigrateToVersion155AddMethodName();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_WEB_APP_MANIFEST_SECTION_TABLE_H_
