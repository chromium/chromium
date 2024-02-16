// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_

#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

namespace plus_addresses {

// Manages plus_address related tables in the Chrome profile scoped "Web Data"
// SQLite database.
// Owned by the `WebDatabaseBackend` managing the "Web Data" database, which is
// owned by the `WebDataServiceWrapper` keyed service.
class PlusAddressTable : public WebDatabaseTable {
 public:
  PlusAddressTable();
  PlusAddressTable(const PlusAddressTable&) = delete;
  PlusAddressTable& operator=(const PlusAddressTable&) = delete;
  ~PlusAddressTable() override;

  // Retrieves the `PlusAddressTable` owned by `db`.
  static PlusAddressTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
