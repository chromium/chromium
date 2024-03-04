// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

class WebDatabaseService;

namespace plus_addresses {

// The PlusAddressWebDataService mirrors `PlusAddressTable`'s API and is
// responsible for posting tasks from the UI sequence to the DB sequence,
// invoking the relevant function on `PlusAddressTable`.
// For read operations, results are returned to a `WebDataServiceConsumer`, who
// must live on the UI sequence.
// Owned by `WebDataServiceWrapper`.
class PlusAddressWebDataService : public WebDataServiceBase {
 public:
  PlusAddressWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // `PlusAddressTable`'s API.
  void GetPlusProfiles(WebDataServiceConsumer* consumer);
  void AddPlusProfile(const PlusProfile& profile);
  void ClearPlusProfiles();

 protected:
  ~PlusAddressWebDataService() override;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_
