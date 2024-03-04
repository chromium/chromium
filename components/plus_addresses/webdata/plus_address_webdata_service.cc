// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_webdata_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"

namespace plus_addresses {

PlusAddressWebDataService::PlusAddressWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebDataServiceBase(std::move(wdbs), ui_task_runner) {}

PlusAddressWebDataService::~PlusAddressWebDataService() = default;

void PlusAddressWebDataService::GetPlusProfiles(
    WebDataServiceConsumer* consumer) {
  wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce([](WebDatabase* db) -> std::unique_ptr<WDTypedResult> {
        return std::make_unique<WDResult<std::vector<PlusProfile>>>(
            PLUS_ADDRESS_RESULT,
            PlusAddressTable::FromWebDatabase(db)->GetPlusProfiles());
      }),
      consumer);
}

void PlusAddressWebDataService::AddPlusProfile(const PlusProfile& profile) {
  auto db_task = base::BindOnce(
      [](const PlusProfile& profile, WebDatabase* db) {
        return PlusAddressTable::FromWebDatabase(db)->AddPlusProfile(profile)
                   ? WebDatabase::COMMIT_NEEDED
                   : WebDatabase::COMMIT_NOT_NEEDED;
      },
      profile);
  wdbs_->ScheduleDBTask(FROM_HERE, std::move(db_task));
}

void PlusAddressWebDataService::ClearPlusProfiles() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce([](WebDatabase* db) {
        return PlusAddressTable::FromWebDatabase(db)->ClearPlusProfiles()
                   ? WebDatabase::COMMIT_NEEDED
                   : WebDatabase::COMMIT_NOT_NEEDED;
      }));
}

}  // namespace plus_addresses
