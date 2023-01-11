// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/region_data_loader_impl.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data_builder.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

RegionDataLoaderImpl::RegionDataLoaderImpl(
    ::i18n::addressinput::Source* address_input_source,
    ::i18n::addressinput::Storage* address_input_storage,
    const std::string& app_locale)
    // region_data_supplier_ takes ownership of source and storage.
    : region_data_supplier_(address_input_source, address_input_storage),
      app_locale_(app_locale) {
  region_data_supplier_callback_.reset(::i18n::addressinput::BuildCallback(
      this, &RegionDataLoaderImpl::OnRegionDataLoaded));
}

RegionDataLoaderImpl::~RegionDataLoaderImpl() = default;

void RegionDataLoaderImpl::LoadRegionData(
    const std::string& country_code,
    RegionDataLoaderImpl::RegionDataLoaded callback) {
  callback_ = callback;
  // This is the first and only time |LoadRules()| is called on the
  // |region_data_supplier_|. This guarantees that the supplied callback
  // |region_data_supplier_callback_| will be invoked resulting in the
  // destruction of this instance.
  // |LoadRules()| may use a network request that has an internal timeout of
  // 5 seconds.
  region_data_supplier_.LoadRules(country_code,
                                  *region_data_supplier_callback_);
}

void RegionDataLoaderImpl::ClearCallback() {
  callback_.Reset();
}

void RegionDataLoaderImpl::OnRegionDataLoaded(bool success,
                                              const std::string& country_code,
                                              int unused_rule_count) {
  if (!callback_.is_null()) {
    if (success) {
      std::string best_region_tree_language_tag;
      ::i18n::addressinput::RegionDataBuilder builder(&region_data_supplier_);
      callback_.Run(
          builder
              .Build(country_code, app_locale_, &best_region_tree_language_tag)
              .sub_regions());
    } else {
      callback_.Run(std::vector<const ::i18n::addressinput::RegionData*>());
    }
  }
  // The deletion must be asynchronous since the caller is not quite done with
  // the preload supplier.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RegionDataLoaderImpl::DeleteThis,
                                base::Unretained(this)));
}

void RegionDataLoaderImpl::DeleteThis() {
  delete this;
}

}  // namespace autofill
