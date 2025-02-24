// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// TODO: crbug.com/393123618 - Implement `PassesDataManager`.
//
// A shared instance of this service is created for regular and off-the-record
// profiles. Future modifications to this service must make sure that no data is
// persisted for the off-the-record profile.
class PassesDataManager : public KeyedService {
 public:
  explicit PassesDataManager(
      scoped_refptr<AutofillWebDataService> webdata_service);
  PassesDataManager(const PassesDataManager&) = delete;
  PassesDataManager& operator=(const PassesDataManager&) = delete;
  ~PassesDataManager() override;

 private:
  const scoped_refptr<AutofillWebDataService> webdata_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
