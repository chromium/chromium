// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_OBSERVER_H_
#define COMPONENTS_UKM_UKM_RECORDER_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"
#include "components/ukm/ukm_consent_state.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {

// Base class for observing UkmRecorderImpl. This object is notified when
// a new UKM entry is added, or when a source URL is updated, or when
// entries are purged. Observers are notified even if |recording_enabled_| is
// false. All the methods are notified on the same SequencedTaskRunner
// on which the observer is added.
class COMPONENT_EXPORT(UKM_RECORDER) UkmRecorderObserver
    : public base::CheckedObserver {
 public:
  UkmRecorderObserver() = default;
  ~UkmRecorderObserver() override = default;
  UkmRecorderObserver(const UkmRecorderObserver&) = delete;
  UkmRecorderObserver& operator=(const UkmRecorderObserver&) = delete;

  // Called when a new UKM entry is added.
  virtual void OnEntryAdded(mojom::UkmEntryPtr entry);

  // Called when URLs are updated for a |source_id|.
  virtual void OnUpdateSourceURL(SourceId source_id,
                                 const std::vector<GURL>& urls);

  // Called when UKM entries related to the given URL scheme should be purged.
  virtual void OnPurgeRecordingsWithUrlScheme(const std::string& url_scheme);

  // Called when all UKM entries should be purged.
  virtual void OnPurge();

  // Called when the UKM consent state is changed in any way, for the new
  // state to take effect. |state| is the collection of accepted consent types.
  // Each consent type is true iff the client has granted this consent on
  // all their Chrome profiles and URL-keyed anonymized data collection is
  // enabled for all profiles.
  virtual void OnUkmAllowedStateChanged(UkmConsentState state);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_OBSERVER_H_
