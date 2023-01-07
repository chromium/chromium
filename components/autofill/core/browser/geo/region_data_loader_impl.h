// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_IMPL_H_

#include "components/autofill/core/browser/geo/region_data_loader.h"

#include <memory>
#include <string>

#include "base/timer/timer.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"

namespace i18n {
namespace addressinput {
class Source;
class Storage;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

// A wrapper for the PreloadSupplier to simplify testing. Until crbug.com/712832
// is fixed, to avoid leaks instances of this class are left hanging until the
// callback is Run. If the callback is never run, this class will leak. But even
// if we would prevent this class from leaking, the preload supplier will leak
// data anyway if the load never completes. This class gives us more opportunity
// to prevent that preload supplier leak if the data load happens after the UI
// timeout waiting for this data expires.
class RegionDataLoaderImpl : public RegionDataLoader {
 public:
  RegionDataLoaderImpl(::i18n::addressinput::Source* address_input_source,
                       ::i18n::addressinput::Storage* address_input_storage,
                       const std::string& app_locale);

  ~RegionDataLoaderImpl() override;

  // RegionDataLoader.
  void LoadRegionData(const std::string& country_code,
                      RegionDataLoader::RegionDataLoaded callback) override;
  void ClearCallback() override;

 private:
  void OnRegionDataLoaded(bool success,
                          const std::string& country_code,
                          int unused_rule_count);
  void DeleteThis();

  // The callback to give to |region_data_supplier_| for async operations.
  std::unique_ptr<::i18n::addressinput::PreloadSupplier::Callback>
      region_data_supplier_callback_;

  // A supplier of region data.
  ::i18n::addressinput::PreloadSupplier region_data_supplier_;

  std::string app_locale_;
  RegionDataLoader::RegionDataLoaded callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_IMPL_H_
