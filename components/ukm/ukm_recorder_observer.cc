// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_observer.h"

namespace ukm {

void UkmRecorderObserver::OnEntryAdded(mojom::UkmEntryPtr entry) {}

void UkmRecorderObserver::OnUpdateSourceURL(SourceId source_id,
                                            const std::vector<GURL>& urls) {}

void UkmRecorderObserver::OnPurgeRecordingsWithUrlScheme(
    const std::string& url_scheme) {}

void UkmRecorderObserver::OnPurge() {}

void UkmRecorderObserver::OnUkmAllowedStateChanged(UkmConsentState state) {}

}  // namespace ukm
