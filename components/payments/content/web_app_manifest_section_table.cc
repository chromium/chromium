// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_app_manifest_section_table.h"

#include <stdint.h>
#include <time.h>
#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace payments {
namespace {

// Data valid duration in seconds.
const time_t WEB_APP_MANIFEST_VALID_TIME_IN_SECONDS = 90 * 24 * 60 * 60;

// Note that the fingerprint is calculated with SHA-256.
const size_t kFingerPrintLength = 32;

WebDatabaseTable::TypeKey GetWebAppManifestKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

// Converts 2-dimensional vector |fingerprints| to 1-dimesional vector.
std::unique_ptr<std::vector<uint8_t>> SerializeFingerPrints(
    const std::vector<std::vector<uint8_t>>& fingerprints) {
  auto serialized_fingerprints = std::make_unique<std::vector<uint8_t>>();

  for (const auto& fingerprint : fingerprints) {
    DCHECK_EQ(fingerprint.size(), kFingerPrintLength);
    serialized_fingerprints->insert(serialized_fingerprints->end(),
                                    fingerprint.begin(), fingerprint.end());
  }

  return serialized_fingerprints;
}

// Converts 1-dimensional vector created by SerializeFingerPrints back to
// 2-dimensional vector. Each vector of the second dimensional vector has exact
// kFingerPrintLength number of elements.
bool DeserializeFingerPrints(
    const std::vector<uint8_t>& fingerprints,
    std::vector<std::vector<uint8_t>>& deserialized_fingerprints) {
  if (fingerprints.size() % kFingerPrintLength != 0)
    return false;

  for (size_t i = 0; i < fingerprints.size(); i += kFingerPrintLength) {
    deserialized_fingerprints.emplace_back(
        fingerprints.begin() + i,
        fingerprints.begin() + i + kFingerPrintLength);
  }
  return true;
}

}  // namespace

WebAppManifestSectionTable::WebAppManifestSectionTable() = default;

WebAppManifestSectionTable::~WebAppManifestSectionTable() = default;

WebAppManifestSectionTable* WebAppManifestSectionTable::FromWebDatabase(
    WebDatabase* db) {
  return static_cast<WebAppManifestSectionTable*>(
      db->GetTable(GetWebAppManifestKey()));
}

WebDatabaseTable::TypeKey WebAppManifestSectionTable::GetTypeKey() const {
  return GetWebAppManifestKey();
}

bool WebAppManifestSectionTable::CreateTablesIfNecessary() {
  if (!db()->Execute("CREATE TABLE IF NOT EXISTS web_app_manifest_section ( "
                     "expire_date INTEGER NOT NULL DEFAULT 0, "
                     "id VARCHAR, "
                     "min_version INTEGER NOT NULL DEFAULT 0, "
                     "fingerprints BLOB) ")) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  return true;
}

bool WebAppManifestSectionTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  return true;
}

void WebAppManifestSectionTable::RemoveExpiredData() {
  const base::Time now = base::Time::NowFromSystemTime();
  sql::Statement s(db()->GetUniqueStatement(
      "DELETE FROM web_app_manifest_section WHERE expire_date < ? "));
  s.BindTime(0, now);
  s.Run();
}

bool WebAppManifestSectionTable::AddWebAppManifest(
    const std::vector<WebAppManifestSection>& manifest) {
  DCHECK_LT(0U, manifest.size());

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  sql::Statement s1(db()->GetUniqueStatement(
      "DELETE FROM web_app_manifest_section WHERE id=? "));
  for (const auto& section : manifest) {
    s1.BindString(0, section.id);
    if (!s1.Run())
      return false;
    s1.Reset(true);
  }

  sql::Statement s2(
      db()->GetUniqueStatement("INSERT INTO web_app_manifest_section "
                               "(expire_date, id, min_version, fingerprints) "
                               "VALUES (?, ?, ?, ?)"));
  const base::Time expire_date =
      base::Time::FromTimeT(base::Time::NowFromSystemTime().ToTimeT() +
                            WEB_APP_MANIFEST_VALID_TIME_IN_SECONDS);
  for (const auto& section : manifest) {
    int index = 0;
    s2.BindTime(index++, expire_date);
    s2.BindString(index++, section.id);
    s2.BindInt64(index++, section.min_version);
    std::unique_ptr<std::vector<uint8_t>> serialized_fingerprints =
        SerializeFingerPrints(section.fingerprints);
    s2.BindBlob(index, *serialized_fingerprints);
    if (!s2.Run())
      return false;
    s2.Reset(true);
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::vector<WebAppManifestSection>
WebAppManifestSectionTable::GetWebAppManifest(const std::string& web_app) {
  sql::Statement s(
      db()->GetUniqueStatement("SELECT id, min_version, fingerprints "
                               "FROM web_app_manifest_section "
                               "WHERE id=?"));
  s.BindString(0, web_app);

  std::vector<WebAppManifestSection> manifest;
  while (s.Step()) {
    WebAppManifestSection section;
    int index = 0;
    section.id = s.ColumnString(index++);
    section.min_version = s.ColumnInt64(index++);

    std::vector<uint8_t> fingerprints;
    if (!s.ColumnBlobAsVector(index, &fingerprints)) {
      manifest.clear();
      break;
    }

    if (!DeserializeFingerPrints(fingerprints, section.fingerprints)) {
      manifest.clear();
      break;
    }

    manifest.emplace_back(std::move(section));
  }

  return manifest;
}

}  // namespace payments
