// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_block_list.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

namespace {

PreviewsEligibilityReason BlocklistReasonToPreviewsReason(
    blocklist::BlocklistReason reason) {
  switch (reason) {
    case blocklist::BlocklistReason::kBlocklistNotLoaded:
      return PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED;
    case blocklist::BlocklistReason::kUserOptedOutInSession:
      return PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT;
    case blocklist::BlocklistReason::kUserOptedOutInGeneral:
      return PreviewsEligibilityReason::USER_BLOCKLISTED;
    case blocklist::BlocklistReason::kUserOptedOutOfHost:
      return PreviewsEligibilityReason::HOST_BLOCKLISTED;
    case blocklist::BlocklistReason::kUserOptedOutOfType:
      NOTREACHED() << "Previews does not support type-base blocklisting";
      return PreviewsEligibilityReason::ALLOWED;
    case blocklist::BlocklistReason::kAllowed:
      return PreviewsEligibilityReason::ALLOWED;
  }
}

}  // namespace

PreviewsBlockList::PreviewsBlockList(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blocklist::OptOutBlocklistDelegate* blocklist_delegate,
    blocklist::BlocklistData::AllowedTypesAndVersions allowed_types)
    : blocklist::OptOutBlocklist(std::move(opt_out_store),
                                 clock,
                                 blocklist_delegate),
      allowed_types_(std::move(allowed_types)) {
  DCHECK(blocklist_delegate);
  Init();
}

bool PreviewsBlockList::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                               size_t* history,
                                               int* threshold) const {
  *duration = params::SingleOptOutDuration();
  *history = 1;
  *threshold = 1;
  return true;
}

bool PreviewsBlockList::ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                                  size_t* history,
                                                  int* threshold) const {
  *history = params::MaxStoredHistoryLengthForHostIndifferentBlockList();
  *threshold = params::HostIndifferentBlockListOptOutThreshold();
  *duration = params::HostIndifferentBlockListPerHostDuration();
  return true;
}
bool PreviewsBlockList::ShouldUseHostPolicy(base::TimeDelta* duration,
                                            size_t* history,
                                            int* threshold,
                                            size_t* max_hosts) const {
  *max_hosts = params::MaxInMemoryHostsInBlockList();
  *history = params::MaxStoredHistoryLengthForPerHostBlockList();
  *threshold = params::PerHostBlockListOptOutThreshold();
  *duration = params::PerHostBlockListDuration();
  return true;
}
bool PreviewsBlockList::ShouldUseTypePolicy(base::TimeDelta* duration,
                                            size_t* history,
                                            int* threshold) const {
  return false;
}

blocklist::BlocklistData::AllowedTypesAndVersions
PreviewsBlockList::GetAllowedTypes() const {
  return allowed_types_;
}

PreviewsBlockList::~PreviewsBlockList() = default;

base::Time PreviewsBlockList::AddPreviewNavigation(const GURL& url,
                                                   bool opt_out,
                                                   PreviewsType type) {
  DCHECK(url.has_host());
  UMA_HISTOGRAM_BOOLEAN("Previews.OptOut.UserOptedOut", opt_out);
  base::BooleanHistogram::FactoryGet(
      base::StringPrintf("Previews.OptOut.UserOptedOut.%s",
                         GetStringNameForType(type).c_str()),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(opt_out);

  return blocklist::OptOutBlocklist::AddEntry(url.host(), opt_out,
                                              static_cast<int>(type));
}

PreviewsEligibilityReason PreviewsBlockList::IsLoadedAndAllowed(
    const GURL& url,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  DCHECK(url.has_host());

  std::vector<blocklist::BlocklistReason> passed_blocklist_reasons;
  blocklist::BlocklistReason reason =
      blocklist::OptOutBlocklist::IsLoadedAndAllowed(
          url.host(), static_cast<int>(type), false, &passed_blocklist_reasons);
  for (auto passed_reason : passed_blocklist_reasons) {
    passed_reasons->push_back(BlocklistReasonToPreviewsReason(passed_reason));
  }

  return BlocklistReasonToPreviewsReason(reason);
}

}  // namespace previews
