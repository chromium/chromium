// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TEST_TEST_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TEST_TEST_TRACKER_H_

#include <memory>

namespace feature_engagement {
class Tracker;
class TrackerEventExporter;

// Provides a test feature_engagement::Tracker that makes all non-relevant
// conditions true so you can test per-feature specific configurations.
// Note: Your feature config params must have |"availability": "ANY"|
// or the FeatureConfigConditionValidator will return false.
std::unique_ptr<Tracker> CreateTestTracker();

// Same as above but accepts event exporter for the tracker as input.
std::unique_ptr<Tracker> CreateTestTracker(
    std::unique_ptr<TrackerEventExporter> event_exporter);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TEST_TEST_TRACKER_H_
