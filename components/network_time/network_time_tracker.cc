// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/client_update_protocol/cup.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// Time updates happen in two ways. First, other components may call
// UpdateNetworkTime() if they happen to obtain the time securely. This will
// likely be deprecated in favor of the second way, which is scheduled time
// queries issued by NetworkTimeTracker itself.
//
// On startup, the clock state may be read from a pref. (This, too, may be
// deprecated.) After that, the time is checked every |kCheckTimeInterval|. A
// "check" means the possibility, but not the certainty, of a time query. A time
// query may be issued at random, or if the network time is believed to have
// become inaccurate.
//
// After issuing a query, the next check will not happen until
// |kBackoffInterval|. This delay is doubled in the event of an error.

namespace network_time {

// Network time queries are enabled on Android and all desktop platforms except
// Chrome OS, which uses tlsdated to set the system time.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kNetworkTimeServiceQuerying, base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kNetworkTimeServiceQuerying, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace {

// Duration between time checks. The value should be greater than zero. Note
// that a "check" is not necessarily a network time query!
constexpr base::FeatureParam<base::TimeDelta> kCheckTimeInterval{
    &kNetworkTimeServiceQuerying, "CheckTimeInterval", base::Seconds(360)};

// Minimum number of minutes between time queries.
constexpr base::FeatureParam<base::TimeDelta> kBackoffInterval{
    &kNetworkTimeServiceQuerying, "BackoffInterval", base::Hours(1)};

// Probability that a check will randomly result in a query. Checks are made
// every |kCheckTimeInterval|. The default values are chosen with the goal of a
// high probability that a query will be issued every 24 hours. The value should
// fall between 0.0 and 1.0 (inclusive).
constexpr base::FeatureParam<double> kRandomQueryProbability{
    &kNetworkTimeServiceQuerying, "RandomQueryProbability", .012};

// The |kFetchBehavior| parameter can have three values:
//
// - "background-only": Time queries will be issued in the background as
//   needed (when the clock loses sync), but on-demand time queries will
//   not be issued (i.e. StartTimeFetch() will not start time queries.)
//
// - "on-demand-only": Time queries will not be issued except when
//   StartTimeFetch() is called. This is the default value.
//
// - "background-and-on-demand": Time queries will be issued both in the
//   background as needed and also on-demand.
constexpr base::FeatureParam<NetworkTimeTracker::FetchBehavior>::Option
    kFetchBehaviorOptions[] = {
        {NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY, "background-only"},
        {NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY, "on-demand-only"},
        {NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
         "background-and-on-demand"},
};
constexpr base::FeatureParam<NetworkTimeTracker::FetchBehavior> kFetchBehavior{
    &kNetworkTimeServiceQuerying, "FetchBehavior",
    NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
    &kFetchBehaviorOptions};

// Number of time measurements performed in a given network time calculation.
constexpr uint32_t kNumTimeMeasurements = 7;

// Maximum time lapse before deserialized data are considered stale.
constexpr uint32_t kSerializedDataMaxAgeDays = 7;

// Name of a pref that stores the wall clock time, via
// |InMillisecondsFSinceUnixEpoch|.
constexpr char kPrefTime[] = "local";

// Name of a pref that stores the tick clock time, via |ToInternalValue|.
constexpr char kPrefTicks[] = "ticks";

// Name of a pref that stores the time uncertainty, via |ToInternalValue|.
constexpr char kPrefUncertainty[] = "uncertainty";

// Name of a pref that stores the network time via
// |InMillisecondsFSinceUnixEpoch|.
constexpr char kPrefNetworkTime[] = "network";

// Time server's maximum allowable clock skew, in seconds.  (This is a property
// of the time server that we happen to know.  It's unlikely that it would ever
// be that badly wrong, but all the same it's included here to document the very
// rough nature of the time service provided by this class.)
constexpr uint32_t kTimeServerMaxSkewSeconds = 10;

constexpr char kTimeServiceURL[] = "http://clients2.google.com/time/1/current";

// This is a DER-encoded ASN.1 SubjectPublicKeyInfo for an ML-DSA-44 key.
constexpr int kKeyVersion = 10;
constexpr auto kPubKey = std::to_array<uint8_t>({
    0x30, 0x82, 0x05, 0x32, 0x30, 0x0B, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x03, 0x11, 0x03, 0x82, 0x05, 0x21, 0x00, 0x2B, 0xE0,
    0x83, 0xFD, 0x20, 0xFB, 0x60, 0x72, 0x41, 0xDA, 0xCC, 0x79, 0xE5, 0x05,
    0x25, 0xAF, 0xBE, 0x01, 0xF3, 0x97, 0x79, 0x7A, 0xD7, 0xBD, 0xF4, 0xC3,
    0x3E, 0xB8, 0x33, 0x8D, 0xBF, 0xDF, 0x50, 0x2D, 0x80, 0x15, 0x29, 0x21,
    0xD2, 0x93, 0x2E, 0x15, 0x00, 0x06, 0x81, 0x8E, 0x8F, 0xB1, 0x25, 0x7E,
    0x8D, 0x0D, 0xBD, 0x73, 0x79, 0xCA, 0x44, 0x2D, 0x1A, 0xC4, 0x17, 0xE6,
    0xF0, 0xE8, 0x1F, 0x62, 0x26, 0xA7, 0x30, 0x79, 0xC1, 0x70, 0xC6, 0x60,
    0x8E, 0x46, 0xDD, 0xC5, 0x8C, 0xA8, 0xC7, 0x82, 0x35, 0x39, 0x68, 0xE3,
    0xD6, 0xB0, 0xCF, 0x15, 0x0E, 0xD7, 0xC7, 0x4D, 0xD8, 0xF1, 0x81, 0x02,
    0x06, 0x52, 0x95, 0x36, 0xA7, 0xE5, 0x58, 0x23, 0xF4, 0x25, 0x3D, 0x9B,
    0x91, 0x10, 0x3F, 0xBD, 0x8F, 0x78, 0x21, 0x20, 0x9F, 0xAE, 0xFA, 0x9A,
    0x64, 0x17, 0x09, 0x9B, 0x35, 0x6B, 0x66, 0x22, 0x19, 0xDF, 0xD4, 0x1E,
    0xF5, 0xB3, 0x14, 0xCC, 0xE8, 0xE4, 0xD8, 0x63, 0xC2, 0x68, 0x48, 0x0C,
    0x99, 0xA8, 0x3B, 0x35, 0xE9, 0xC5, 0x55, 0xDE, 0x60, 0x65, 0x55, 0xEA,
    0x1C, 0xF9, 0x8B, 0x19, 0xF4, 0xEB, 0xFB, 0x2E, 0x5F, 0x2A, 0x7F, 0xE2,
    0x39, 0x88, 0xF3, 0x95, 0x77, 0xBB, 0x1C, 0x18, 0x72, 0xD8, 0x27, 0x7E,
    0x1B, 0xD6, 0x95, 0xA4, 0x50, 0x23, 0xE0, 0xDA, 0xBA, 0xED, 0x8B, 0xC2,
    0x14, 0x6E, 0xB4, 0xA9, 0x56, 0x05, 0x09, 0x8C, 0x82, 0x54, 0x57, 0x5D,
    0xF6, 0xEB, 0x9E, 0x7F, 0x53, 0x55, 0xA1, 0xBA, 0x4E, 0x9C, 0xDD, 0xB4,
    0x9F, 0x22, 0x5C, 0x7B, 0x50, 0xC3, 0xA9, 0x8F, 0xC8, 0x2C, 0x69, 0xE4,
    0x46, 0x42, 0x06, 0xF2, 0x6F, 0x1B, 0x27, 0x1B, 0x67, 0x86, 0x53, 0x01,
    0x5F, 0xD8, 0x61, 0x28, 0x59, 0x5E, 0x2A, 0xD8, 0xDF, 0xBD, 0x6C, 0xFF,
    0x6C, 0x5E, 0xEE, 0x8C, 0x97, 0x1D, 0x66, 0x7A, 0xDB, 0x00, 0x83, 0xF8,
    0x79, 0xED, 0x57, 0x7D, 0xB2, 0x49, 0xB3, 0x8F, 0xDE, 0x85, 0x69, 0x84,
    0x84, 0x38, 0x84, 0x96, 0x67, 0x2F, 0x8F, 0x3D, 0x98, 0xEE, 0x5F, 0xD2,
    0x53, 0x5E, 0x49, 0x85, 0xAD, 0xD7, 0x5E, 0xAB, 0x40, 0xBD, 0x49, 0x94,
    0x20, 0xBD, 0x3B, 0xCA, 0xDA, 0x1F, 0xFD, 0x7B, 0xA9, 0x3B, 0xD9, 0xEC,
    0x8A, 0x8E, 0x65, 0x1C, 0x67, 0xBB, 0x8E, 0x16, 0x6A, 0x22, 0x1D, 0x5C,
    0xCE, 0x6D, 0x10, 0xC7, 0x3B, 0x3B, 0xCE, 0x84, 0xA3, 0xF3, 0xAF, 0x5F,
    0x3C, 0xB5, 0xEC, 0xF9, 0x9C, 0x63, 0xAD, 0x2D, 0x95, 0x86, 0xB4, 0x2D,
    0x11, 0xE0, 0x13, 0xB2, 0xAE, 0xDF, 0xCB, 0xCD, 0x97, 0x93, 0xEA, 0x90,
    0xF0, 0x3F, 0x0B, 0x8A, 0xC6, 0xED, 0xB8, 0xF6, 0xDE, 0xAB, 0x65, 0x0D,
    0xD7, 0xAD, 0xF7, 0x31, 0x51, 0x6A, 0xED, 0x59, 0x8C, 0x93, 0xD4, 0x93,
    0xAB, 0x41, 0x1E, 0xAD, 0x41, 0xC1, 0x4D, 0xA7, 0xBA, 0xF4, 0xCD, 0x31,
    0x5D, 0xC5, 0x49, 0x20, 0xDC, 0x8B, 0xA1, 0xA3, 0xC7, 0x7B, 0x47, 0x53,
    0x67, 0x82, 0xE0, 0x69, 0xEC, 0xE2, 0xFB, 0xFE, 0xE8, 0x47, 0x8E, 0x28,
    0x00, 0x61, 0x65, 0x1B, 0x23, 0xEF, 0x9D, 0x41, 0x7B, 0xC9, 0xB1, 0x58,
    0x85, 0xB8, 0xFA, 0x17, 0x0C, 0x02, 0xFD, 0x8C, 0x63, 0x3D, 0x70, 0xF6,
    0x91, 0xCA, 0x55, 0xB6, 0x03, 0xA8, 0x44, 0x53, 0x12, 0x14, 0xAF, 0xB2,
    0xB2, 0x6E, 0xC4, 0x81, 0x79, 0xBF, 0x51, 0xE3, 0x51, 0xCC, 0x93, 0x75,
    0x20, 0xDF, 0x48, 0x90, 0x10, 0x17, 0x13, 0x3D, 0x41, 0x95, 0xFA, 0xF1,
    0x40, 0x32, 0x5A, 0x04, 0x1C, 0xF1, 0xC6, 0x91, 0x6A, 0x55, 0x32, 0xF1,
    0xA5, 0xD5, 0xFA, 0x54, 0xC7, 0xE1, 0x35, 0xE1, 0xFA, 0x6B, 0xA4, 0x60,
    0x54, 0xCA, 0xAD, 0x30, 0xA3, 0x91, 0x55, 0x1E, 0xA6, 0xA9, 0x76, 0x11,
    0x87, 0x3A, 0x9F, 0xAC, 0xA6, 0x8A, 0xB6, 0xDB, 0x7C, 0x1C, 0xB1, 0x82,
    0x84, 0xD8, 0x5F, 0x48, 0xC9, 0xFC, 0x83, 0x70, 0xA6, 0x41, 0xA6, 0xC3,
    0x67, 0x0D, 0x65, 0xBC, 0x8C, 0xD2, 0x44, 0x46, 0x17, 0x85, 0xA0, 0xE9,
    0x2A, 0x67, 0x87, 0x52, 0xB4, 0x0D, 0xD4, 0xEB, 0x31, 0x13, 0x6D, 0x63,
    0xD8, 0x23, 0x0D, 0x54, 0xC9, 0x06, 0x15, 0x25, 0x1A, 0xF8, 0xEF, 0xF8,
    0x39, 0x12, 0xE8, 0xC6, 0x9E, 0xB0, 0x8D, 0xDC, 0xDE, 0xB4, 0x08, 0xFF,
    0x65, 0xDA, 0x07, 0xDF, 0x3D, 0xBE, 0x75, 0x6D, 0xF0, 0xCF, 0xD4, 0x4F,
    0xD1, 0x8D, 0xC6, 0x44, 0x5C, 0x8D, 0x98, 0x0F, 0x9C, 0x0D, 0x10, 0x74,
    0x07, 0x39, 0x42, 0xE8, 0xF7, 0x8F, 0x7C, 0xA0, 0x1E, 0xE2, 0xAA, 0xC5,
    0xEC, 0x72, 0x89, 0x74, 0xED, 0xF4, 0xA4, 0x71, 0x3A, 0xBD, 0x18, 0x93,
    0xB8, 0x89, 0x5B, 0x88, 0x77, 0x0C, 0x42, 0x30, 0x65, 0xDD, 0x8A, 0xCC,
    0xF2, 0xD1, 0x2A, 0x3B, 0x0A, 0x65, 0x30, 0xF3, 0x98, 0xDA, 0x32, 0x8A,
    0xB8, 0xAF, 0x63, 0x3A, 0xC8, 0xBD, 0x8A, 0x24, 0xFF, 0x1C, 0x2D, 0x56,
    0x5D, 0xA7, 0x7A, 0x5D, 0xC6, 0x8E, 0x8F, 0xA2, 0x76, 0xBB, 0x4C, 0xDC,
    0x1A, 0xD8, 0xD8, 0xDA, 0xEB, 0xED, 0x93, 0xD2, 0x58, 0x29, 0x25, 0xD5,
    0xEB, 0x6D, 0xF7, 0xD4, 0xEA, 0x75, 0x99, 0x80, 0x2D, 0x8C, 0xCF, 0x68,
    0x7A, 0xBB, 0xAC, 0x2B, 0x44, 0xF9, 0x1B, 0x71, 0x31, 0xF3, 0x3E, 0xED,
    0x3C, 0x05, 0x45, 0x27, 0xD4, 0x72, 0x13, 0x44, 0x5E, 0x84, 0xAA, 0x4E,
    0x19, 0xAC, 0xE4, 0xAC, 0xC4, 0xD4, 0xDF, 0x69, 0x04, 0x16, 0xDC, 0xCC,
    0x0D, 0x5F, 0x2E, 0xBD, 0x5B, 0xC4, 0xFD, 0xD0, 0x5F, 0x3B, 0x94, 0x34,
    0x28, 0xAF, 0x67, 0xD0, 0x64, 0x3C, 0xEF, 0x57, 0x46, 0xF2, 0x6A, 0x17,
    0x0B, 0x02, 0x6A, 0x96, 0xD2, 0xD9, 0x2C, 0x1B, 0x03, 0xB3, 0x33, 0x06,
    0x7F, 0x02, 0x0D, 0xC6, 0xB0, 0x40, 0x3E, 0xFE, 0x6D, 0x8C, 0x7A, 0xC1,
    0xF5, 0x20, 0x4C, 0xBD, 0xAD, 0x0C, 0x32, 0x8E, 0xD6, 0x12, 0x9A, 0x28,
    0xE5, 0xE1, 0xC6, 0x6D, 0xFA, 0x4F, 0xA7, 0xCE, 0x2C, 0x49, 0x82, 0x4F,
    0x8E, 0xA7, 0x5A, 0x46, 0xBF, 0x19, 0x0A, 0x51, 0x55, 0xA3, 0xCD, 0x9A,
    0x9F, 0xE3, 0x7A, 0x6D, 0xE0, 0xF9, 0x6C, 0x73, 0xC1, 0x1B, 0xA6, 0x82,
    0xFF, 0x79, 0x45, 0xA3, 0x53, 0x8E, 0x53, 0xF8, 0x48, 0xA6, 0xF9, 0x77,
    0x85, 0xA3, 0x3E, 0xEC, 0x96, 0xEE, 0xA6, 0x2E, 0xE7, 0x99, 0x9C, 0x3C,
    0x1B, 0x48, 0x16, 0x1F, 0x66, 0x58, 0xE9, 0xC2, 0x0D, 0x92, 0x2E, 0x40,
    0x31, 0xBC, 0x14, 0xBF, 0x76, 0xA8, 0xA1, 0x76, 0x5C, 0xE4, 0x5B, 0x42,
    0x64, 0xBF, 0x42, 0xB1, 0x1C, 0x09, 0xBF, 0xD9, 0xB9, 0xE7, 0xCE, 0x82,
    0x4A, 0x7B, 0x07, 0xCA, 0xED, 0xF9, 0xBC, 0x70, 0x4E, 0x16, 0x4E, 0x19,
    0xD4, 0x4D, 0xB9, 0xAC, 0x1B, 0x32, 0x16, 0x8D, 0x52, 0x17, 0xF1, 0xA3,
    0x64, 0xFE, 0xD3, 0xF7, 0x47, 0x97, 0x1D, 0x06, 0x9A, 0x4D, 0xDF, 0x90,
    0xC1, 0x80, 0x12, 0xF6, 0x74, 0xAD, 0x0E, 0x5F, 0xA8, 0x88, 0x1A, 0x8C,
    0x78, 0x96, 0xB1, 0xDC, 0x68, 0x9B, 0x41, 0x8D, 0xBD, 0x18, 0x3C, 0x3C,
    0x93, 0xF7, 0x80, 0xE9, 0x3B, 0x50, 0x50, 0x6B, 0x0E, 0x47, 0x37, 0x99,
    0xAB, 0x79, 0xD0, 0xBC, 0xA2, 0x45, 0x73, 0x51, 0xAD, 0x77, 0xEF, 0x0A,
    0x54, 0xE1, 0xB6, 0xDA, 0x17, 0x45, 0xA4, 0xCE, 0x2D, 0x11, 0xA2, 0xD8,
    0x0E, 0x87, 0xFA, 0x51, 0x04, 0x52, 0xFE, 0x3E, 0x5B, 0x84, 0x05, 0xA0,
    0x9D, 0x55, 0x8D, 0x66, 0x9E, 0x15, 0x6B, 0x4B, 0x54, 0xE3, 0xB5, 0x0A,
    0x60, 0x26, 0x99, 0x6A, 0x98, 0xBD, 0xB2, 0xC6, 0x1A, 0x29, 0xF5, 0x83,
    0x12, 0x5B, 0x2A, 0xDB, 0x35, 0xC4, 0xE5, 0x23, 0x15, 0x2D, 0x6E, 0xF9,
    0xA8, 0x8E, 0xD0, 0xA5, 0xB5, 0xEA, 0xAD, 0xB6, 0x9E, 0x75, 0xCB, 0x02,
    0xD3, 0xA5, 0xE3, 0x4F, 0x78, 0x2F, 0x13, 0xD5, 0x35, 0x89, 0xED, 0x64,
    0x66, 0xBD, 0x13, 0x26, 0x18, 0x2C, 0x82, 0x6C, 0x85, 0x8A, 0x0B, 0xE6,
    0x49, 0x8D, 0xEA, 0x49, 0x16, 0x64, 0x88, 0xC9, 0xE6, 0xFC, 0x06, 0xC2,
    0xDD, 0x73, 0xE7, 0xDB, 0xB4, 0x2F, 0xFE, 0x5B, 0x61, 0x79, 0xE9, 0x10,
    0xA7, 0x60, 0x39, 0x26, 0x23, 0xFB, 0xAB, 0x5E, 0x62, 0x06, 0x0A, 0x49,
    0x11, 0xAE, 0xF2, 0xB6, 0xEB, 0x15, 0x27, 0xA3, 0xD8, 0xC3, 0x15, 0x9A,
    0x1D, 0x5B, 0x87, 0x69, 0x60, 0xB8, 0x04, 0x78, 0x0F, 0x4D, 0xE4, 0x3D,
    0x69, 0x36, 0x6A, 0x04, 0x98, 0x7D, 0xB9, 0x2A, 0x59, 0x2D, 0x99, 0x23,
    0x97, 0x2F, 0x09, 0x51, 0x2B, 0x43, 0xE7, 0x1F, 0xAE, 0x8C, 0xE0, 0x62,
    0x8A, 0xEE, 0x9F, 0xD8, 0x48, 0xFF, 0x86, 0x9B, 0xB8, 0xC8, 0x2D, 0x89,
    0xFF, 0x17, 0xE7, 0xAE, 0xFE, 0x9C, 0x0F, 0x30, 0xCA, 0x4A, 0x5A, 0xCF,
    0xB2, 0x7D, 0x3E, 0x82, 0x08, 0xB2, 0xB8, 0xEF, 0x96, 0xC7, 0x41, 0x8E,
    0xA8, 0x3B, 0x23, 0x52, 0x83, 0x1C, 0x5B, 0x1B, 0x38, 0x6D, 0x05, 0xA9,
    0xAF, 0xCF, 0x3B, 0x4B, 0x74, 0x98, 0xC3, 0xC8, 0x6E, 0x5A, 0x9A, 0x21,
    0x3F, 0xC5, 0xD1, 0xD0, 0x91, 0x0B, 0xF6, 0x86, 0x98, 0x33, 0x03, 0x8C,
    0x7A, 0x7E, 0xDF, 0x50, 0xD7, 0x52, 0xD7, 0xDD, 0x48, 0xBB, 0xA4, 0xF1,
    0x8E, 0x38, 0xC2, 0xCF, 0xE8, 0x46, 0x9D, 0x4D, 0x89, 0x50, 0x70, 0x38,
    0x58, 0x9E, 0x14, 0x8B, 0x33, 0x1D, 0x09, 0x70, 0xE8, 0xB0, 0x3B, 0xF3,
    0xFD, 0xB4, 0x8E, 0xD4, 0xA3, 0x5F, 0x14, 0xB4, 0x49, 0x19, 0xB4, 0xA3,
    0x0E, 0x49, 0x9B, 0xF4, 0xE3, 0x36, 0x44, 0x59, 0x3F, 0x3D, 0xDB, 0x3C,
    0xDD, 0x7F, 0xE5, 0x18, 0x39, 0x15, 0x41, 0x93, 0x32, 0xBD, 0xEF, 0x14,
    0xE2, 0xDE,
});

std::string GetServerProof(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  std::string proof;
  return response_headers->EnumerateHeader(nullptr, "x-cup-server-proof",
                                           &proof)
             ? proof
             : std::string();
}

}  // namespace

NetworkTimeTracker::NetworkTimeObserver::NetworkTimeObserver(
    NetworkTimeTracker* tracker)
    : tracker_(tracker) {
  CHECK(tracker_);
  tracker_->AddObserver(this);
}

void NetworkTimeTracker::NetworkTimeObserver::OnNetworkTimeTrackerDestroyed(
    NetworkTimeTracker* tracker) {
  CHECK_EQ(tracker_, tracker);
  tracker_->RemoveObserver(this);
  tracker_ = nullptr;
}

NetworkTimeTracker::NetworkTimeObserver::~NetworkTimeObserver() {
  if (tracker_) {
    tracker_->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());
}

// static
void NetworkTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkTimeMapping);
  registry->RegisterBooleanPref(prefs::kNetworkTimeQueriesEnabled, true);
}

NetworkTimeTracker::NetworkTimeTracker(
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<const base::TickClock> tick_clock,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::optional<FetchBehavior> fetch_behavior,
    base::span<const uint8_t> pubkey)
    : server_url_(kTimeServiceURL),
      max_response_size_(1024),
      query_signer_(kKeyVersion, pubkey.empty() ? kPubKey : pubkey),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      time_query_completed_(false),
      fetch_behavior_(fetch_behavior) {
  // If `pref_service` is null, defer the remaining initialization. This allows
  // the NetworkTimeTracker to be created, and subscribed-to, very early in
  // startup, before the PrefService, NetworkService, FieldTrials, etc are
  // fully initialized.
  if (pref_service) {
    Initialize(pref_service, std::move(url_loader_factory));
  }
}

NetworkTimeTracker::~NetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_) {
    observer.OnNetworkTimeTrackerDestroyed(this);
  }
  CHECK(observers_.empty());
}

void NetworkTimeTracker::Initialize(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!is_initialized());
  CHECK(pref_service);
  pref_service_ = pref_service;

  url_loader_factory_ = std::move(url_loader_factory);

  // Set the backoff interval to the default value. This is done here, instead
  // of in the constructor, because the backoff interval is a feature parameter
  // whose value is not known until the feature list is initialized.
  backoff_ = kBackoffInterval.Get();

  // Finish initialization by checking whether network time mapping data is
  // available in the prefs, and if so, use it to initialize the time tracker.
  const base::DictValue& time_mapping =
      pref_service_->GetDict(prefs::kNetworkTimeMapping);
  std::optional<double> time_js = time_mapping.FindDouble(kPrefTime);
  std::optional<double> ticks_js = time_mapping.FindDouble(kPrefTicks);
  std::optional<double> uncertainty_js =
      time_mapping.FindDouble(kPrefUncertainty);
  std::optional<double> network_time_js =
      time_mapping.FindDouble(kPrefNetworkTime);
  if (time_js && ticks_js && uncertainty_js && network_time_js) {
    base::Time time_at_last_measurement =
        base::Time::FromMillisecondsSinceUnixEpoch(*time_js);
    base::TimeTicks ticks_at_last_measurement =
        base::TimeTicks::FromInternalValue(static_cast<int64_t>(*ticks_js));
    base::TimeDelta network_time_uncertainty =
        base::TimeDelta::FromInternalValue(
            static_cast<int64_t>(*uncertainty_js));
    base::Time network_time_at_last_measurement =
        base::Time::FromMillisecondsSinceUnixEpoch(*network_time_js);
    base::Time now = clock_->Now();
    if (ticks_at_last_measurement > tick_clock_->NowTicks() ||
        time_at_last_measurement > now ||
        now - time_at_last_measurement >
            base::Days(kSerializedDataMaxAgeDays)) {
      // Drop saved mapping if either clock has run backward, or the data are
      // too old.
      pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    } else {
      tracker_.emplace(time_at_last_measurement, ticks_at_last_measurement,
                       network_time_at_last_measurement,
                       network_time_uncertainty);
    }
  }

  // Start the loop to check the time.
  QueueCheckTime(base::Seconds(0));
}

void NetworkTimeTracker::UpdateNetworkTime(base::Time network_time,
                                           base::TimeDelta resolution,
                                           base::TimeDelta latency,
                                           base::TimeTicks post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << base::UTF16ToUTF8(
                  base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_at_last_measurement_|.
  base::Time network_time_at_last_measurement = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now_ticks - post_time;
  DCHECK_GE(task_delay.InMilliseconds(), 0);
  DCHECK_GE(latency.InMilliseconds(), 0);
  // Estimate that the time was set midway through the latency time.
  base::TimeDelta offset = task_delay + latency / 2;
  base::TimeTicks ticks_at_last_measurement = now_ticks - offset;
  base::Time time_at_last_measurement = clock_->Now() - offset;

  // Can't assume a better time than the resolution of the given time and the
  // ticks measurements involved, each with their own uncertainty.  1 & 2 are
  // the ones used to compute the latency, 3 is the Now() from when this task
  // was posted, 4 and 5 are the Now() and NowTicks() above, and 6 and 7 will be
  // the Now() and NowTicks() in GetNetworkTime().
  base::TimeDelta network_time_uncertainty =
      resolution + latency +
      kNumTimeMeasurements * base::Milliseconds(kTicksResolutionMs);

  tracker_.emplace(time_at_last_measurement, ticks_at_last_measurement,
                   network_time_at_last_measurement, network_time_uncertainty);

  base::UmaHistogramMediumTimes("NetworkTime.NetworkTimeUncertainty",
                                network_time_uncertainty);

  base::DictValue time_mapping;
  time_mapping.Set(kPrefTime,
                   time_at_last_measurement.InMillisecondsFSinceUnixEpoch());
  time_mapping.Set(
      kPrefTicks,
      static_cast<double>(ticks_at_last_measurement.ToInternalValue()));
  time_mapping.Set(
      kPrefUncertainty,
      static_cast<double>(network_time_uncertainty.ToInternalValue()));
  time_mapping.Set(
      kPrefNetworkTime,
      network_time_at_last_measurement.InMillisecondsFSinceUnixEpoch());
  pref_service_->Set(prefs::kNetworkTimeMapping,
                     base::Value(std::move(time_mapping)));

  NotifyObservers();
}

bool NetworkTimeTracker::AreTimeFetchesEnabled() const {
  return is_initialized() &&
         base::FeatureList::IsEnabled(kNetworkTimeServiceQuerying);
}

NetworkTimeTracker::FetchBehavior NetworkTimeTracker::GetFetchBehavior() const {
  return fetch_behavior_.value_or(kFetchBehavior.Get());
}

void NetworkTimeTracker::SetTimeServerURLForTesting(const GURL& url) {
  server_url_ = url;
}

GURL NetworkTimeTracker::GetTimeServerURLForTesting() const {
  return server_url_;
}

void NetworkTimeTracker::SetMaxResponseSizeForTesting(size_t limit) {
  max_response_size_ = limit;
}

bool NetworkTimeTracker::QueryTimeServiceForTesting() {
  CheckTime();
  return time_fetcher_ != nullptr;
}

void NetworkTimeTracker::WaitForFetch() {
  base::RunLoop run_loop;
  fetch_completion_callbacks_.push_back(run_loop.QuitClosure());
  run_loop.Run();
}

void NetworkTimeTracker::AddObserver(NetworkTimeObserver* obs) {
  observers_.AddObserver(obs);
}

void NetworkTimeTracker::RemoveObserver(NetworkTimeObserver* obs) {
  observers_.RemoveObserver(obs);
}

bool NetworkTimeTracker::GetTrackerState(
    TimeTracker::TimeTrackerState* state) const {
  base::Time unused;
  auto res = GetNetworkTime(&unused, nullptr);
  if (res != NETWORK_TIME_AVAILABLE) {
    return false;
  }
  *state = tracker_->GetStateAtCreation();
  return true;
}

void NetworkTimeTracker::WaitForFetchForTesting(uint32_t nonce) {
  query_signer_.OverrideNonceForTesting(kKeyVersion, nonce);  // IN-TEST
  WaitForFetch();
}

void NetworkTimeTracker::OverrideNonceForTesting(uint32_t nonce) {
  query_signer_.OverrideNonceForTesting(kKeyVersion, nonce);  // IN-TEST
}

base::TimeDelta NetworkTimeTracker::GetTimerDelayForTesting() const {
  DCHECK(timer_.IsRunning());
  return timer_.GetCurrentDelay();
}

void NetworkTimeTracker::ClearNetworkTimeForTesting() {
  tracker_ = std::nullopt;
}

NetworkTimeTracker::NetworkTimeResult NetworkTimeTracker::GetNetworkTime(
    base::Time* network_time,
    base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (!tracker_.has_value()) {
    if (time_query_completed_) {
      // Time query attempts have been made in the past and failed.
      if (time_fetcher_) {
        // A fetch (not the first attempt) is in progress.
        return NETWORK_TIME_SUBSEQUENT_SYNC_PENDING;
      }
      return NETWORK_TIME_NO_SUCCESSFUL_SYNC;
    }
    // No time queries have happened yet.
    if (time_fetcher_) {
      return NETWORK_TIME_FIRST_SYNC_PENDING;
    }
    return NETWORK_TIME_NO_SYNC_ATTEMPT;
  }

  if (!tracker_->GetTime(clock_->Now(), tick_clock_->NowTicks(), network_time,
                         uncertainty)) {
    return NETWORK_TIME_SYNC_LOST;
  }
  return NETWORK_TIME_AVAILABLE;
}

bool NetworkTimeTracker::StartTimeFetch(base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior != FETCHES_ON_DEMAND_ONLY &&
      behavior != FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    return false;
  }

  // Enqueue the callback before calling CheckTime(), so that if
  // CheckTime() completes synchronously, the callback gets called.
  fetch_completion_callbacks_.push_back(std::move(closure));

  // If a time query is already in progress, do not start another one.
  if (time_fetcher_) {
    return true;
  }

  // Cancel any fetches that are scheduled for the future, and try to
  // start one now.
  timer_.Stop();
  CheckTime();

  // CheckTime() does not necessarily start a fetch; for example, time
  // queries might be disabled or network time might already be
  // available.
  if (!time_fetcher_) {
    // If no query is in progress, no callbacks need to be called.
    fetch_completion_callbacks_.clear();
    return false;
  }
  return true;
}

void NetworkTimeTracker::CheckTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeDelta interval = kCheckTimeInterval.Get();
  if (interval.is_negative()) {
    interval = kCheckTimeInterval.default_value;
  }

  // If NetworkTimeTracker is waking up after a backoff, this will reset the
  // timer to its default faster frequency.
  QueueCheckTime(interval);

  if (!ShouldIssueTimeQuery()) {
    return;
  }

  std::string query_string =
      query_signer_.PrepareRequestParameters(/*request_body=*/"");
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  GURL url = server_url_.ReplaceComponents(replacements);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("network_time_component", R"(
        semantics {
          sender: "Network Time Component"
          description:
            "Sends a request to a Google server to retrieve the current "
            "timestamp."
          trigger:
            "A request can be sent to retrieve the current time when the user "
            "encounters an SSL date error, or in the background if Chromium "
            "determines that it doesn't have an accurate timestamp."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            BrowserNetworkTimeQueriesEnabled {
                BrowserNetworkTimeQueriesEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  // Not expecting any cookies, but just in case.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->enable_load_timing = true;
  // This cancels any outstanding fetch.
  time_fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);
  time_fetcher_->SetAllowHttpErrorResults(true);
  time_fetcher_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NetworkTimeTracker::OnURLLoaderComplete,
                     base::Unretained(this)),
      max_response_size_);

  timer_.Stop();  // Restarted in OnURLLoaderComplete().
}

bool NetworkTimeTracker::UpdateTimeFromResponse(
    std::optional<std::string> response_body) {
  int response_code = 0;
  if (time_fetcher_->ResponseInfo() && time_fetcher_->ResponseInfo()->headers) {
    response_code = time_fetcher_->ResponseInfo()->headers->response_code();
  }
  if (response_code != 200 || !response_body) {
    time_query_completed_ = true;
    DVLOG(1) << "fetch failed code=" << response_code;
    return false;
  }

  std::string_view response(*response_body);

  if (!query_signer_.ValidateResponse(
          response, GetServerProof(time_fetcher_->ResponseInfo()->headers))) {
    DVLOG(1) << "invalid signature";
    return false;
  }
  response.remove_prefix(5);  // Skips leading )]}'\n
  std::optional<base::DictValue> value = base::JSONReader::ReadDict(
      response, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value) {
    DVLOG(1) << "not a dictionary";
    return false;
  }
  std::optional<double> current_time_millis =
      value->FindDouble("current_time_millis");
  if (!current_time_millis) {
    DVLOG(1) << "no current_time_millis";
    return false;
  }

  // There is a "server_nonce" key here too, but it serves no purpose other than
  // to make the server's response unpredictable.
  base::Time current_time =
      base::Time::FromMillisecondsSinceUnixEpoch(*current_time_millis);
  base::TimeDelta resolution =
      base::Milliseconds(1) + base::Seconds(kTimeServerMaxSkewSeconds);

  // Record histograms for the latency of the time query and the time delta
  // between time fetches.
  base::TimeDelta latency =
      time_fetcher_->ResponseInfo()->load_timing.receive_headers_start -
      time_fetcher_->ResponseInfo()->load_timing.send_end;

  last_fetched_time_ = current_time;

  UpdateNetworkTime(current_time, resolution, latency, tick_clock_->NowTicks());
  return true;
}

void NetworkTimeTracker::OnURLLoaderComplete(
    std::optional<std::string> response_body) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(time_fetcher_);

  time_query_completed_ = true;

  // After completion of a query, whether succeeded or failed, go to sleep
  // for a long time.
  if (!UpdateTimeFromResponse(
          std::move(response_body))) {  // On error, back off.
    if (backoff_ < base::Days(2)) {
      backoff_ *= 2;
    }
  } else {
    backoff_ = kBackoffInterval.Get();
  }
  QueueCheckTime(backoff_);
  time_fetcher_.reset();

  // Clear |fetch_completion_callbacks_| before running any of them,
  // because a callback could call StartTimeFetch() to enqueue another
  // callback.
  std::vector<base::OnceClosure> callbacks =
      std::move(fetch_completion_callbacks_);
  fetch_completion_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void NetworkTimeTracker::QueueCheckTime(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta()) << "delay must be non-negative";
  // Check if the user is opted in to background time fetches.
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior == FETCHES_IN_BACKGROUND_ONLY ||
      behavior == FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    timer_.Start(FROM_HERE, delay,
                 base::BindRepeating(&NetworkTimeTracker::CheckTime,
                                     base::Unretained(this)));
  }
}

bool NetworkTimeTracker::ShouldIssueTimeQuery() {
  // Do not query the time service if the feature is not enabled.
  if (!AreTimeFetchesEnabled()) {
    return false;
  }

  // Do not query the time service if queries are disabled by policy.
  if (!pref_service_->GetBoolean(prefs::kNetworkTimeQueriesEnabled)) {
    return false;
  }

  // If GetNetworkTime() does not return NETWORK_TIME_AVAILABLE,
  // synchronization has been lost and a query is needed.
  base::Time network_time;
  if (GetNetworkTime(&network_time, nullptr) != NETWORK_TIME_AVAILABLE) {
    return true;
  }

  // Otherwise, make the decision at random.
  double probability = kRandomQueryProbability.Get();
  if (probability < 0.0 || probability > 1.0) {
    probability = kRandomQueryProbability.default_value;
  }

  return base::RandDouble() < probability;
}

void NetworkTimeTracker::NotifyObservers() {
  // Don't notify if the current state is not NETWORK_TIME_AVAILABLE.
  base::Time unused;
  auto res = GetNetworkTime(&unused, nullptr);
  if (res != NETWORK_TIME_AVAILABLE) {
    return;
  }
  TimeTracker::TimeTrackerState state = tracker_->GetStateAtCreation();
  for (NetworkTimeObserver& obs : observers_) {
    obs.OnNetworkTimeChanged(state);
  }
}

}  // namespace network_time
