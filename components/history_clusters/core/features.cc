// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

}  // namespace

namespace internal {

const base::Feature kJourneys{"Journeys", enabled_by_default_desktop_only};

const base::Feature kJourneysLabels{"JourneysLabel",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOmniboxAction{"JourneysOmniboxAction",
                                   enabled_by_default_desktop_only};

const base::Feature kNonUserVisibleDebug{"JourneysNonUserVisibleDebug",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserVisibleDebug{"JourneysUserVisibleDebug",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPersistContextAnnotationsInHistoryDb{
    "JourneysPersistContextAnnotationsInHistoryDb",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHistoryClustersInternalsPage{
    "HistoryClustersInternalsPage", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHistoryClustersUseContinueOnShutdown{
    "HistoryClustersUseContinueOnShutdown", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace internal

}  // namespace history_clusters
