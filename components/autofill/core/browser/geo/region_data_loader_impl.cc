// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/region_data_loader_impl.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
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

RegionDataLoaderImpl::~RegionDataLoaderImpl() {}

void RegionDataLoaderImpl::LoadRegionData(
    const std::string& country_code,
    RegionDataLoaderImpl::RegionDataLoaded callback,
    int64_t timeout_ms) {
  callback_ = callback;
  region_data_supplier_.LoadRules(country_code,
                                  *region_data_supplier_callback_);

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_ms),
               base::BindOnce(&RegionDataLoaderImpl::OnRegionDataLoaded,
                              base::Unretained(this), false, country_code, 0));
}

void RegionDataLoaderImpl::ClearCallback() {
  callback_.Reset();
}

void RegionDataLoaderImpl::OnRegionDataLoaded(bool success,
                                              const std::string& country_code,
                                              int unused_rule_count) {
  timer_.Stop();
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RegionDataLoaderImpl::DeleteThis,
                                base::Unretained(this)));
}

void RegionDataLoaderImpl::DeleteThis() {
  delete this;
}

}  // namespace autofill
