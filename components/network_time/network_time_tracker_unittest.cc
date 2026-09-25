// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

namespace {

struct MockedResponse {
  network::mojom::URLResponseHeadPtr head;
  std::string body;
  network::URLLoaderCompletionStatus status;
};

constexpr auto kDevKeyPubBytes = std::to_array<uint8_t>({
    0x30, 0x82, 0x05, 0x32, 0x30, 0x0B, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x03, 0x11, 0x03, 0x82, 0x05, 0x21, 0x00, 0x70, 0x43,
    0xFB, 0x88, 0x56, 0x36, 0x2E, 0xB9, 0x09, 0xB9, 0x24, 0xED, 0x1E, 0x29,
    0x10, 0x04, 0x00, 0x59, 0x4A, 0xA3, 0x84, 0x36, 0x29, 0x2A, 0x94, 0xA7,
    0x66, 0xAE, 0x61, 0x4E, 0xB7, 0x8E, 0xFC, 0x19, 0x8D, 0x4B, 0xE5, 0xB6,
    0x84, 0x57, 0xB5, 0x3C, 0xA2, 0xB0, 0x97, 0x50, 0x76, 0xAF, 0x3E, 0xA9,
    0x74, 0xE7, 0xDF, 0x6F, 0x2C, 0x3A, 0x4D, 0x94, 0x1E, 0xCA, 0xCA, 0xB5,
    0x8A, 0x94, 0xB4, 0x97, 0xA2, 0xAA, 0x3F, 0x51, 0x46, 0xFD, 0xFC, 0x86,
    0x45, 0x61, 0x90, 0x9F, 0x71, 0x65, 0x12, 0xE8, 0xBB, 0xFF, 0x14, 0xA4,
    0x50, 0xB0, 0x8E, 0x33, 0x13, 0x87, 0x3F, 0xE1, 0x73, 0x73, 0xB0, 0x82,
    0xF2, 0x19, 0x4D, 0xCF, 0x83, 0xAC, 0x80, 0xEE, 0xBD, 0x9B, 0x58, 0xF3,
    0xD3, 0x2B, 0x53, 0x0E, 0x58, 0x88, 0x30, 0x0D, 0x85, 0x33, 0xB2, 0x24,
    0x78, 0xE0, 0xDC, 0xE5, 0x36, 0x9A, 0x6B, 0x51, 0xFD, 0x36, 0x87, 0x36,
    0x07, 0x63, 0x34, 0xFE, 0x29, 0x41, 0x00, 0xB2, 0xD4, 0xA9, 0x48, 0xDF,
    0x1B, 0xA6, 0x9E, 0xCC, 0x79, 0x47, 0xC0, 0x30, 0x1B, 0x23, 0x36, 0x8A,
    0x79, 0xE1, 0x84, 0x0C, 0x4E, 0xF8, 0x15, 0x46, 0x2F, 0xE1, 0x3A, 0x63,
    0xCA, 0xF7, 0xF3, 0xD4, 0xAD, 0xDE, 0xAE, 0xFB, 0x49, 0x11, 0xF7, 0x37,
    0xC4, 0x9E, 0x78, 0x98, 0xDB, 0xFC, 0x67, 0x0E, 0x17, 0x0F, 0xAC, 0xC9,
    0x8E, 0xAB, 0xFD, 0x7D, 0x47, 0xAD, 0xCC, 0x5F, 0x04, 0x4F, 0xE5, 0xAC,
    0x07, 0x8A, 0x05, 0x9F, 0x41, 0x06, 0x63, 0xB9, 0x8E, 0xE4, 0x43, 0xF9,
    0xC0, 0x42, 0x8F, 0x0A, 0x38, 0x57, 0x7A, 0x8D, 0xAF, 0x73, 0x39, 0x97,
    0xE1, 0x00, 0x5B, 0x4D, 0x3D, 0x9E, 0xEA, 0xEE, 0x8C, 0xED, 0x88, 0x5B,
    0x8A, 0x9A, 0x72, 0xC1, 0x07, 0x26, 0x88, 0xEA, 0x09, 0x98, 0x69, 0xA1,
    0x2A, 0xC6, 0x94, 0xCC, 0x1B, 0xA3, 0x39, 0x9C, 0xF3, 0x71, 0xE5, 0x44,
    0x9A, 0xDE, 0xC6, 0x83, 0xE8, 0xBD, 0x8D, 0xDD, 0x46, 0xBD, 0x04, 0x03,
    0xFC, 0xD6, 0x15, 0x5A, 0xF5, 0xD6, 0x06, 0x07, 0x24, 0xAF, 0x2A, 0xAF,
    0xFF, 0x83, 0x92, 0x03, 0x6D, 0x9A, 0x3F, 0xD6, 0x5F, 0xEA, 0x11, 0x84,
    0x23, 0x24, 0x89, 0x3E, 0x09, 0x0D, 0xBD, 0xC5, 0xE0, 0xC3, 0x93, 0xFA,
    0x02, 0x6B, 0x19, 0x13, 0xC3, 0xA0, 0x2D, 0x53, 0x06, 0x4E, 0x68, 0x03,
    0x80, 0xA4, 0x06, 0x7F, 0x79, 0x33, 0x0A, 0x33, 0x57, 0xEF, 0x48, 0x43,
    0xBD, 0x35, 0x2E, 0xFA, 0x17, 0x19, 0xD3, 0x83, 0xDF, 0x08, 0x81, 0xF5,
    0xED, 0x11, 0x5A, 0xBC, 0x69, 0x36, 0x3B, 0xF2, 0x81, 0x7D, 0x0A, 0xAD,
    0xE2, 0x53, 0x18, 0x00, 0x05, 0xA2, 0xA0, 0x4F, 0xF5, 0xEF, 0x3E, 0x1E,
    0x3C, 0xBB, 0x09, 0x1F, 0x31, 0x29, 0x67, 0x70, 0x54, 0xA4, 0x51, 0x3D,
    0x27, 0xB3, 0xD7, 0xC5, 0x25, 0xBA, 0x5F, 0x58, 0x5C, 0xE3, 0x5C, 0x62,
    0xAC, 0x50, 0xEF, 0xEC, 0x6A, 0x70, 0xCF, 0x4F, 0x1B, 0xEB, 0xAB, 0xB8,
    0x23, 0x7E, 0xCC, 0x9D, 0x39, 0x0E, 0xE9, 0xE3, 0x5B, 0x13, 0xC3, 0x05,
    0xD7, 0xF5, 0xA2, 0xA5, 0xA0, 0xE2, 0x55, 0xFD, 0x7A, 0xEF, 0xFF, 0x52,
    0x7B, 0x90, 0x57, 0xBD, 0xBB, 0x45, 0x3F, 0x59, 0x2D, 0x6B, 0xE7, 0x31,
    0x8B, 0xC7, 0x20, 0x1D, 0xA8, 0xD4, 0x38, 0xA8, 0xEE, 0x7A, 0xFC, 0xA4,
    0xE4, 0x30, 0x94, 0xFA, 0x5B, 0xE2, 0x46, 0xE5, 0xC0, 0x35, 0x47, 0x5C,
    0x71, 0x39, 0x24, 0x98, 0x9D, 0x63, 0xFB, 0x0D, 0x51, 0xD2, 0xF1, 0xE5,
    0x80, 0xDB, 0xE5, 0x93, 0xC8, 0xCB, 0x3D, 0xAA, 0x35, 0x3C, 0x37, 0x8E,
    0x90, 0x06, 0x64, 0xDE, 0xFC, 0x02, 0x32, 0x28, 0x6D, 0xA0, 0x90, 0x4B,
    0x77, 0x6A, 0x6E, 0x02, 0xE1, 0xD3, 0x35, 0xE0, 0xA3, 0x25, 0x3A, 0x30,
    0xD2, 0x0D, 0x54, 0x86, 0xF1, 0x72, 0x81, 0xC6, 0x9F, 0xB9, 0x1A, 0x63,
    0xB7, 0x3D, 0x6D, 0x15, 0x53, 0xBD, 0x67, 0xCC, 0x5C, 0x7C, 0x5B, 0x1C,
    0x33, 0x46, 0x09, 0xCE, 0x7B, 0x2D, 0xE8, 0x78, 0x3B, 0xFC, 0xC9, 0xEC,
    0x7E, 0xFB, 0x0D, 0x25, 0x75, 0x05, 0x39, 0x09, 0x28, 0x29, 0xB4, 0x56,
    0x91, 0x05, 0x8E, 0x2F, 0x60, 0x0D, 0x0A, 0x9F, 0xB2, 0x12, 0x21, 0x55,
    0x44, 0xEC, 0x00, 0x36, 0x55, 0x9C, 0xC4, 0x56, 0x7A, 0xCC, 0x5F, 0x53,
    0xC0, 0xA7, 0x48, 0x38, 0x19, 0x6D, 0x72, 0xA8, 0x63, 0xDA, 0xA5, 0x69,
    0x1B, 0xC5, 0xB4, 0xEA, 0xFA, 0x75, 0x97, 0x25, 0xB2, 0x57, 0x1D, 0x5B,
    0xDE, 0xC5, 0x86, 0x3A, 0x9B, 0xB9, 0x9D, 0x15, 0x6D, 0x5B, 0xC5, 0xA5,
    0xBE, 0x40, 0xD4, 0xBA, 0xD3, 0xB7, 0x74, 0x82, 0x21, 0xAF, 0x4C, 0x0E,
    0xC3, 0x17, 0x8E, 0x17, 0xDA, 0x19, 0xA4, 0x70, 0x27, 0x14, 0xBD, 0x8A,
    0x88, 0xEF, 0x23, 0x11, 0xB0, 0x63, 0x4A, 0xE0, 0xF4, 0xEE, 0xAE, 0x76,
    0xE2, 0xF9, 0x7C, 0xCB, 0x67, 0xBA, 0x9B, 0x07, 0x2B, 0x9F, 0x07, 0xA9,
    0x70, 0x4E, 0x87, 0x32, 0x51, 0x3F, 0xE2, 0xFB, 0x38, 0xED, 0x72, 0xF2,
    0xF6, 0x2A, 0x14, 0x97, 0x48, 0x2B, 0x74, 0xBD, 0xB1, 0xAD, 0x27, 0x6A,
    0x30, 0x02, 0x39, 0x30, 0x1C, 0x17, 0x7A, 0x17, 0x91, 0x1A, 0x90, 0xCD,
    0xC6, 0x42, 0x9B, 0x82, 0xBB, 0x85, 0xF6, 0xD7, 0xEF, 0x38, 0x57, 0xFD,
    0x85, 0xC2, 0x38, 0xBE, 0xF5, 0x30, 0x55, 0x5E, 0x3D, 0x72, 0xE6, 0x11,
    0x16, 0x9F, 0x04, 0xD0, 0x44, 0xC7, 0x2A, 0x58, 0x70, 0xA6, 0x12, 0xBE,
    0x31, 0x83, 0x09, 0xA8, 0x52, 0x21, 0xD8, 0x20, 0x7F, 0x36, 0xE8, 0xC4,
    0x84, 0xC6, 0xB3, 0x84, 0xC7, 0x3E, 0x8F, 0xC9, 0xEC, 0x53, 0x18, 0xA6,
    0x50, 0x9E, 0x60, 0xF6, 0xF1, 0xCD, 0x5E, 0xD6, 0x0A, 0xF7, 0xFB, 0xC7,
    0xE0, 0x06, 0xCD, 0xDA, 0x52, 0xCE, 0x71, 0xA7, 0xF3, 0x9F, 0x36, 0xE7,
    0x91, 0x7E, 0x19, 0xC8, 0x20, 0x8E, 0x95, 0x72, 0xEE, 0x69, 0x36, 0x11,
    0xFA, 0xD8, 0x9D, 0x8F, 0x06, 0xF4, 0x73, 0xAF, 0x66, 0xEC, 0xA0, 0x1D,
    0x0A, 0xEB, 0x44, 0x39, 0xA4, 0x01, 0x39, 0x54, 0x10, 0xE8, 0x39, 0x88,
    0xE5, 0xA4, 0xAE, 0xF1, 0xD6, 0xBE, 0xB3, 0x4A, 0x97, 0x01, 0x78, 0xE8,
    0xA5, 0xFD, 0x0E, 0xAA, 0xA8, 0x03, 0x0D, 0x5D, 0xAE, 0x1B, 0xFB, 0xB3,
    0x05, 0xD0, 0xC6, 0xB0, 0x68, 0x34, 0x8A, 0xD5, 0x3E, 0x45, 0x11, 0x9C,
    0xE8, 0xBC, 0xF1, 0xE0, 0xD9, 0x62, 0x38, 0xA8, 0xA5, 0x21, 0x16, 0xE1,
    0x7B, 0xEB, 0x90, 0xCE, 0x23, 0x90, 0x8F, 0xAC, 0x9A, 0x7D, 0xB1, 0x3E,
    0x1E, 0xE4, 0xBD, 0xFF, 0xE8, 0xA0, 0x50, 0xF5, 0x72, 0xCC, 0x64, 0x72,
    0x8D, 0xE5, 0xE6, 0x67, 0x2C, 0xAA, 0x05, 0x0B, 0xC1, 0x33, 0x5E, 0x56,
    0x3B, 0x58, 0x54, 0xD2, 0x5D, 0x8A, 0xF9, 0x1D, 0x4D, 0x62, 0x5E, 0x75,
    0x44, 0x6C, 0xE3, 0x0C, 0x0A, 0xA2, 0xD2, 0x80, 0x50, 0x2E, 0x32, 0x89,
    0x8E, 0x25, 0xF6, 0x8A, 0x57, 0xBE, 0x9C, 0x92, 0x4B, 0x32, 0xFB, 0x11,
    0xF3, 0xEE, 0xCA, 0xA8, 0x00, 0xDC, 0x3E, 0x51, 0x68, 0x98, 0x6D, 0x32,
    0xCC, 0x59, 0x85, 0xEB, 0x94, 0xF9, 0x33, 0xAB, 0xC2, 0x65, 0x94, 0x14,
    0x4B, 0xAB, 0xB5, 0x87, 0x87, 0xDD, 0x37, 0x7B, 0x4E, 0x46, 0xAD, 0x29,
    0xD2, 0x98, 0x28, 0x3E, 0x42, 0xEC, 0x9D, 0xA5, 0x38, 0x41, 0x8E, 0xF3,
    0x3C, 0xF1, 0xA5, 0xC9, 0xFD, 0x4B, 0x70, 0x43, 0x3A, 0x9F, 0x3B, 0x02,
    0x6B, 0x7B, 0x5A, 0x05, 0x2E, 0x50, 0xB7, 0x30, 0x71, 0xEC, 0x8E, 0x90,
    0x6C, 0x8A, 0xF0, 0x63, 0x5B, 0xCE, 0x82, 0x65, 0x39, 0x5A, 0x2A, 0xB3,
    0x92, 0x90, 0xF5, 0xCA, 0x21, 0x63, 0xB0, 0x23, 0x6A, 0x50, 0xD7, 0x2A,
    0xCE, 0xEE, 0x70, 0x89, 0x7C, 0x1E, 0x33, 0x5C, 0x38, 0x7E, 0xC4, 0xF1,
    0x98, 0xE1, 0x98, 0xFF, 0x6B, 0xC6, 0x3D, 0xEE, 0x2D, 0xB0, 0x61, 0xFC,
    0x07, 0x50, 0xC2, 0xB2, 0xC8, 0x4C, 0xDB, 0xE3, 0x00, 0xDE, 0x92, 0xCF,
    0xAC, 0x6D, 0xE9, 0x68, 0xBE, 0x07, 0xA3, 0x8A, 0x30, 0xE3, 0xC4, 0x01,
    0xCB, 0x23, 0x50, 0x90, 0x49, 0x2B, 0x80, 0x9F, 0x68, 0xDD, 0x88, 0xB5,
    0xF3, 0xEB, 0x12, 0x8D, 0xC4, 0x54, 0x54, 0x03, 0x8E, 0x97, 0x26, 0x6E,
    0xCA, 0xAD, 0xBE, 0x2C, 0x55, 0x44, 0x36, 0x65, 0x53, 0x57, 0x5A, 0xEA,
    0xBD, 0xE8, 0x9C, 0x44, 0x87, 0x58, 0x8D, 0x0D, 0xD1, 0x68, 0x09, 0x2E,
    0x57, 0xE8, 0x9E, 0xAB, 0xDC, 0x81, 0x84, 0x8F, 0x0D, 0xA1, 0x6B, 0x62,
    0x6D, 0xAA, 0xBA, 0xBB, 0xF8, 0x16, 0xCA, 0xBA, 0x7B, 0xAC, 0x0E, 0xDF,
    0xDD, 0xFF, 0x3D, 0x7A, 0x0F, 0xB4, 0x10, 0x07, 0x1B, 0xF2, 0xB3, 0x52,
    0xB2, 0x0E, 0xD9, 0x75, 0xFD, 0x39, 0xA6, 0x0A, 0xA9, 0x6C, 0x92, 0xEB,
    0xC0, 0x85, 0x5A, 0x0E, 0x92, 0x34, 0xF4, 0xBF, 0x58, 0x87, 0xA6, 0xE2,
    0x44, 0x88, 0x2D, 0xAF, 0x84, 0x2F, 0xD6, 0x6A, 0x26, 0x12, 0xD6, 0x45,
    0xD2, 0xB2, 0x9E, 0x4D, 0x86, 0x9B, 0x02, 0x2F, 0x77, 0x24, 0xC8, 0x0E,
    0x7C, 0x36, 0x05, 0x85, 0x7F, 0xEC, 0xA2, 0x5B, 0x13, 0x2A, 0xA4, 0x54,
    0x29, 0xE4, 0x52, 0x38, 0x77, 0x97, 0x13, 0x76, 0xDC, 0x40, 0xBC, 0x2F,
    0xB9, 0x46, 0xC6, 0xBE, 0xA9, 0xEA, 0xA7, 0x7D, 0x0E, 0x30, 0x22, 0x4D,
    0x86, 0x53, 0xE6, 0xDC, 0x8E, 0xD8, 0x5F, 0x6E, 0x1F, 0x91, 0x7F, 0xAF,
    0x90, 0x70, 0xC7, 0x72, 0xF5, 0x40, 0xF3, 0xFE, 0x63, 0x53, 0xFF, 0x48,
    0xBA, 0x34, 0x30, 0x74, 0xA2, 0x76, 0xA1, 0x48, 0x94, 0x61, 0x36, 0xA7,
    0xEB, 0x27, 0x98, 0xC2, 0x8F, 0x09, 0x04, 0x60, 0x5C, 0xFB, 0x8B, 0x8D,
    0xD7, 0x8E,
});

}  // namespace

class NetworkTimeTrackerTest : public ::testing::Test {
 public:
  class NetworkTimeTestObserver
      : public NetworkTimeTracker::NetworkTimeObserver {
   public:
    using Super = NetworkTimeTracker::NetworkTimeObserver;
    explicit NetworkTimeTestObserver(NetworkTimeTracker* tracker)
        : Super(tracker) {}
    ~NetworkTimeTestObserver() override = default;

    void OnNetworkTimeChanged(TimeTracker::TimeTrackerState state) override {
      times_called_++;
      last_state_ = state;
    }

    void OnNetworkTimeTrackerDestroyed(NetworkTimeTracker* tracker) override {
      Super::OnNetworkTimeTrackerDestroyed(tracker);
      times_tracker_destroyed_++;
    }

    int times_called_ = 0;
    int times_tracker_destroyed_ = 0;
    TimeTracker::TimeTrackerState last_state_;
  };

  ~NetworkTimeTrackerTest() override = default;

  NetworkTimeTrackerTest() : NetworkTimeTrackerTest(false) {}
  explicit NetworkTimeTrackerTest(bool dev_keys)
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        field_trial_test_(std::make_unique<FieldTrialTest>()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock) {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    field_trial_test_->SetFeatureParams(
        true, /*query_probability=*/0.0,
        NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);

    url_loader_factory_.SetInterceptor(base::BindRepeating(
        &NetworkTimeTrackerTest::Intercept, weak_ptr_factory_.GetWeakPtr()));

    tracker_ = std::make_unique<NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_),
        std::nullopt, dev_keys ? kDevKeyPubBytes : base::span<const uint8_t>());

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::Days(111));
    tick_clock_->Advance(base::Days(222));
  }

  // Sets `response_handler` as handler for all requests made through
  // `url_loader_factory_`.
  void SetResponseHandler(
      base::RepeatingCallback<MockedResponse()> response_handler) {
    response_handler_ = std::move(response_handler);
  }

  // Replaces |tracker_| with a new object, while preserving the
  // testing clocks.
  void Reset(std::optional<NetworkTimeTracker::FetchBehavior> behavior) {
    base::SimpleTestClock* new_clock = new base::SimpleTestClock();
    new_clock->SetNow(clock_->Now());
    base::SimpleTestTickClock* new_tick_clock = new base::SimpleTestTickClock();
    new_tick_clock->SetNowTicks(tick_clock_->NowTicks());
    clock_ = new_clock;
    tick_clock_ = new_tick_clock;
    tracker_ = std::make_unique<NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_),
        behavior);
  }

  // Good signature over invalid data, though made with a non-production key.
  static MockedResponse BadDataResponseHandler() {
    static constexpr std::string_view kProof =
        "8f9b5c9b6b2b701578bc3d90b6e9a117616ba8da4a6b51ed14a8d1e2ce4b5a6aaf0ba9"
        "0f4a8dc6f8bdc8ef510585ced3c8a15476842ae4991cc5e11ab696513e8d6dde40a79a"
        "dce26d389d33ee77e62e6c5cd8448c446343787b62995e8169a412cd9aa894e14aa367"
        "8c647ea7eec493008b2fc1315bfbae7da0eea27f829d829e6506f371ae687439cfe37e"
        "e07a567bada31f1e5eb2a1e36fc892eb56d6f7cebae2ee4d42ab2ee4e3d6ab28a3ce20"
        "542e90caa04a289ef3697d08e192f12131d520ccfcd91d4d9c4ff1681deb253180a4fc"
        "ce9f43b47ef442e9edc4319d0e42dad87607abcd3cbdda1bb3696e08c87798458cbbc9"
        "ac0d516724976e377b254152d3c62a9de891c0bef4a89449abf2f4ae33718f632f7d30"
        "821417573c5ae6f300bc6ca1d2514ea57cb1cbfea76b99cec3ea8926928e1323f68dc3"
        "da579b6ca35e6162102e460609689c13b15af4428e14cff269ff5091a820b5165485ac"
        "bd0d9d4440f4bfdc40e2ea1e9829c608cb5aafe4e8fcdcf12cc6594c0cd2338728a535"
        "aac36677624c7a64870c56cd69ee58ccf56b74365bcf55211ce10697339b9ccd391fa6"
        "f5c52eca849ab3b2e12acf4097ee8493407efe5b51567365aeef3f9838fa90d2fda194"
        "8463e1d75a6114251000e054759bd50288e42d181f14a3d37acd3da5fec72d01d576f5"
        "f27cfaa4e999fca3ce4d13010d33bafe3a2524ec7fe1234251fb959ea05b4c2e94f5d9"
        "0e8f2d5e70d1aa30d00129e721c2963f935f1f7086f9c944223fc983524ce4de51538d"
        "b40a5eb4b826f7054e4f2453320f5d0695339a2631fa4b40f8a13a306f79219e0a890c"
        "55fd728c7f5f316659c052e93961596aa280b80da2aa0aefdef4a97d0aa0dbb92932a9"
        "3a57596dcd5221978e399e7fc30c18b38d7676e7d8bb9c7b5a968db8514648965df30a"
        "598beffa8578e63a4cdd9fdc6feb04660bb17837ac2f1cde8e4abbef2863e5f24ba24f"
        "4d17a3277b556e32cc8921ded39ec411d0c042d48c5df226935f1794b735bf8b9f844e"
        "2d7dbd8c3c87ac5723ddb98d0ac5a79f201fb47971c544a599b244e8878f9daa398474"
        "2c6b523a3de1fab5116382e932610eb193f295d589268747db258a045abaa565c061f5"
        "dabdd25b33025a99525d160762a08c556e8bfaec9785baf56f99b6fe72d50b8c782171"
        "e85e388d2e51e5a81ca12a8fae379b83866cd6ce071241aebf2e68032752d2acac7517"
        "ef6e7cbd86a5fa73838e8c30e28d3dd120d348ae2afbba0f57f415c6183cb9d3633cbd"
        "a6413b8d62ba5088490598fc063c35b7b20e2a6ad0ad1419799483c01019fccef01f47"
        "6d18f63dffffae46809f690007aa8e9305153a48730d8f13cf16acf043d16be5b9c583"
        "16e786ac99a3dda45355f5075f8d685441f30af2083be3662603328f004da43b34c153"
        "269ea65f6ed83ffb8e86d6a6ff97b79f7b94d1589f87c3badcc63817c6118620c61b1f"
        "3ef755755cef857cf2852e3ad9f5bc6a65a822a10d06c46be4a105f0142522f5d69588"
        "91af7ba7da605cbe45994552e99d8958efc24e7486b9c901ebc9d74541d0766ef0d843"
        "7698adcc10324058d4a6855806a04777aa737632be4f4bd43fbcbe7a146e5c52e934c7"
        "23aa014f640b45e68bf67d902953fb7d2e35193e98ad29acf05cbc8e56e24ef2629c41"
        "3806f7ec62360bcbc9471a5fbdcbe56d94fe07427fa3d8339aba4d6603ccbbaf446eba"
        "e21fc821fd83eba787d4b82845f4d19974ec56527049ba7d7d7453adf36fba78ade070"
        "522db4e327008e289176ad414228dfa66516d599eb0cefb0a61029397db62f072ce52b"
        "77025bc3c6b5a71e209049d9673de18e44a82cd5c9cf3c05dc2f331766621219164ca0"
        "3d4562e2f7b3573d8e3fa07b799e2d811d3e3669e5ec2635e4ab02cbcadd776ff130e4"
        "dfdca19ed3dcb42a6d1b300f39e78ff6fd903a2272b802c84468f0050611e32b1dd900"
        "edf91416eb97ad3e542ab1d286c844c6a128cc425428efde247c863c359a09cc297b32"
        "9a9d88c89c2916bdde47627725489f43490e63f7e293185dfa39979a4de17410bfea1f"
        "4327d24d24770a695265612e835c12db4a402f4d4fd87d6f59f6c65e2ab68eaf3e904d"
        "d76289ce7540e5a87e9354ed164ac725ed6beb153754b5069f3552594cc15ba7ed1504"
        "a3285f12ae5260139354ec27face5bd71285077059680577de289f109cfe0ec5a747bc"
        "8576dd8c9446a591caa09211636d417c2f1df42f4fbff46e1b60379023fc66e82757b9"
        "8131090cbe54e8cc6addffb8ee9dc4d2361d7c571a8ff1d31d1c28ba2a21fd4745326f"
        "50c51bd7d385126b49401cf47529d3ec8e11e51a2917189bf85bb3150fd1075b767204"
        "37a1ab282de44cacdd5fe118aec3c6b29d1c956e753c04e7ac965074368428d1defda8"
        "bd944949c95a6ecb59ad0ea2868b081fb615bbfe43420ea00feb4c1fe8fe14277b90c6"
        "155ee55644dd1806e7c270f2c79af91523f190c243322bda6c0c03c69691fb313a15a3"
        "bd210bb56dfee4cdeb0c4b04b52e5e7de3e2493c2588e9e26fdcf2b8fb93b3df357ef7"
        "b79a5c1985432d38e130b973068f4015678b667736f830aea7dba0fd81490bde8c36f9"
        "6823195c0c606264672ced19f329eeca3efa3094a3b53c6bf19c4915f3bfae69574089"
        "e8149685c4da98c96c3a401f008aea75d36ad58c78c078b2ce6924a08a72856cbd07d3"
        "b2d497004dfe6d9b3841abd0fd3d3c0a7bcfa0667663d76f785ddcdb88692c17a45327"
        "522566feb8eda05bdc7f06970e0e900f22ad5b32af72f47206113ad48f0b1cd9bedbc8"
        "5ab066a8fe008af4d81749d4ef5fffb1bce32e0e9bf7956ed885da7be2c47e94246e34"
        "05a16f9df3dbc211c0f64ee639d4987d454bb44f0e1c26958433caf498e66cb020c12f"
        "0132d426d50ea6ef325d4eac026d15a3aed1ecf0ea9c0deefee6a0093bd855ef0dd6ba"
        "c53ef33045404d70c36f3a7cef1ebc30c3b88e3a28c68b0c76d1706e28bc20d724b526"
        "d1a219289eead3821767cdb74a25843fff0846bb72e81062900aa7721d2083012a18b0"
        "8e616205832ccb2a9df3c3fdc7234d84b18f5354feb80d4348bb086bb56be49bb66eaa"
        "f925fa4ad9f1a6097d786889924b095760bb719390e25bfbe0e01f5410c25ff051161b"
        "9991a020abbe2323371eb6ec7713b09c174082fd5b233ed4c788eeffc88bfc88bab4cc"
        "af0dde6409834cd969c48b6fa3834b4ec5507d9dae1de2c294c767718e09ad53cee2e0"
        "25836346bf80b271d11848146f9b6675a52059eaf8a3efc6c282181a21808794b4c2cf"
        "f2172f41444953788993aad1e0e10c223236383a4b5b72798d91a2aac9f03b41424e50"
        "52545c797e8895b2b6c2d0dae000000000000000000000000000000000000000000000"
        "00000000a172739:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    std::string body =
        ")]}'\n"
        "{\"current_time_millis\":NaN,\"server_nonce\":7.243537120735732E-235}";
    head->headers->AddHeader("x-cup-server-proof", kProof);
    return MockedResponse{std::move(head), std::move(body)};
  }

  static MockedResponse GoodTimeResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    head->headers->AddHeader("x-cup-server-proof",
                             kGoodTimeResponseServerProofHeader[0]);
    return MockedResponse{std::move(head), kGoodTimeResponseBody[0]};
  }

  static MockedResponse BadSignatureResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    std::string body =
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}";
    head->headers->AddHeader("x-cup-server-proof", "dead:beef");
    return MockedResponse{std::move(head), std::move(body)};
  }

  static MockedResponse ServerErrorResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR);
    return MockedResponse{std::move(head), ""};
  }

  static MockedResponse NetworkErrorResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    return MockedResponse{
        std::move(head), "",
        network::URLLoaderCompletionStatus(net::ERR_EMPTY_RESPONSE)};
  }

  // Updates the notifier's time with the specified parameters.
  void UpdateNetworkTime(base::Time network_time,
                         base::TimeDelta resolution,
                         base::TimeDelta latency,
                         base::TimeTicks post_time) {
    tracker_->UpdateNetworkTime(network_time, resolution, latency, post_time);
  }

  // Advances both the system clock and the tick clock.  This should be used for
  // the normal passage of time, i.e. when neither clock is doing anything odd.
  void AdvanceBoth(base::TimeDelta delta) {
    tick_clock_->Advance(delta);
    clock_->Advance(delta);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FieldTrialTest> field_trial_test_;
  // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
  base::TimeDelta resolution_ = base::Milliseconds(17);
  base::TimeDelta latency_ = base::Milliseconds(50);
  base::TimeDelta adjustment_ = 7 * base::Milliseconds(kTicksResolutionMs);
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NetworkTimeTracker> tracker_;
  raw_ptr<base::SimpleTestClock> clock_;
  raw_ptr<base::SimpleTestTickClock> tick_clock_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::RepeatingCallback<MockedResponse()> response_handler_;

 private:
  void Intercept(const network::ResourceRequest& request) {
    CHECK(response_handler_);
    MockedResponse response = response_handler_.Run();
    // status.decoded_body_length = response.body.size();
    url_loader_factory_.AddResponse(request.url, std::move(response.head),
                                    std::move(response.body),
                                    std::move(response.status));
  }

  base::WeakPtrFactory<NetworkTimeTrackerTest> weak_ptr_factory_{this};
};

class NetworkTimeTrackerWithDevKeysTest : public NetworkTimeTrackerTest {
 public:
  NetworkTimeTrackerWithDevKeysTest() : NetworkTimeTrackerTest(true) {}
};

TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&network_time, &uncertainty));
}

TEST_F(NetworkTimeTrackerTest, LongPostingDelay) {
  // The request arrives at the server, which records the time.  Advance the
  // clock to simulate the latency of sending the reply, which we'll say for
  // convenience is half the total latency.
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_ / 2);

  // Record the tick counter at the time the reply is received.  At this point,
  // we would post UpdateNetworkTime to be run on the browser thread.
  base::TimeTicks posting_time = tick_clock_->NowTicks();

  // Simulate that it look a long time (1888us) for the browser thread to get
  // around to executing the update.
  AdvanceBoth(base::Microseconds(1888));
  UpdateNetworkTime(in_network_time, resolution_, latency_, posting_time);

  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
  EXPECT_EQ(clock_->Now(), out_network_time);
}

TEST_F(NetworkTimeTrackerTest, LopsidedLatency) {
  // Simulate that the server received the request instantaneously, and that all
  // of the latency was in sending the reply.  (This contradicts the assumption
  // in the code.)
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_);
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // But, the answer is still within the uncertainty bounds!
  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_LT(out_network_time - uncertainty / 2, clock_->Now());
  EXPECT_GT(out_network_time + uncertainty / 2, clock_->Now());
}

TEST_F(NetworkTimeTrackerTest, ClockIsWack) {
  // Now let's assume the system clock is completely wrong.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(NetworkTimeTrackerTest, ClocksDivergeSlightly) {
  // The two clocks are allowed to diverge a little bit.
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::TimeDelta small = base::Seconds(30);
  tick_clock_->Advance(small);
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + small, out_network_time);
  // The clock divergence should show up in the uncertainty.
  EXPECT_EQ(resolution_ + latency_ + adjustment_ + small, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  // Verify that the the tracker receives and properly handles updates to the
  // network time.
  base::Time out_network_time;
  base::TimeDelta uncertainty;

  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // Fake a wait to make sure we keep tracking.
  AdvanceBoth(base::Seconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // And one more time.
  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  AdvanceBoth(base::Seconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
}

TEST_F(NetworkTimeTrackerTest, SpringForward) {
  // Simulate the wall clock advancing faster than the tick clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Seconds(1));
  clock_->Advance(base::Days(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, TickClockSpringsForward) {
  // Simulate the tick clock advancing faster than the wall clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Days(1));
  clock_->Advance(base::Seconds(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, FallBack) {
  // Simulate the wall clock running backward.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Seconds(1));
  clock_->Advance(base::Days(-1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SuspendAndResume) {
  // Simulate the wall clock advancing while the tick clock stands still, as
  // would happen in a suspend+resume cycle.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  clock_->Advance(base::Hours(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, Serialize) {
  // Test that we can serialize and deserialize state and get consistent
  // results.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);

  // 6 days is just under the threshold for discarding data.
  base::TimeDelta delta = base::Days(6);
  AdvanceBoth(delta);
  Reset(std::nullopt);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + delta, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, DeserializeOldFormat) {
  // Test that deserializing old data (which do not record the uncertainty and
  // tick clock) causes the serialized data to be ignored.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  std::optional<double> local, network;
  const base::DictValue& saved_prefs =
      pref_service_.GetDict(prefs::kNetworkTimeMapping);
  local = saved_prefs.FindDouble("local");
  network = saved_prefs.FindDouble("network");
  ASSERT_TRUE(local);
  ASSERT_TRUE(network);
  base::DictValue prefs;
  prefs.Set("local", *local);
  prefs.Set("network", *network);
  pref_service_.Set(prefs::kNetworkTimeMapping, base::Value(std::move(prefs)));
  Reset(std::nullopt);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithLongDelay) {
  // Test that if the serialized data are more than a week old, they are
  // discarded.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  AdvanceBoth(base::Days(8));
  Reset(std::nullopt);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithTickClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  tick_clock_->Advance(base::Days(1));
  Reset(std::nullopt);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithWallClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  clock_->Advance(base::Days(1));
  Reset(std::nullopt);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetwork) {
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // First query should happen soon.
  EXPECT_EQ(base::Minutes(0), tracker_->GetTimerDelayForTesting());

  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is
  // milliseconds, any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, StartTimeFetch) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_TRUE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
  tracker_->WaitForFetchForTesting(123123123);
  run_loop.Run();

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is milliseconds,
  // any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

// Tests that when StartTimeFetch() is called with a query already in
// progress, it calls the callback when that query completes.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWithQueryInProgress) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());

  base::RunLoop run_loop;
  EXPECT_TRUE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
  tracker_->WaitForFetchForTesting(123123123);
  run_loop.Run();

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is milliseconds,
  // any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

// Tests that StartTimeFetch() returns false if called while network
// time is available.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWhileSynced) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced.
  base::RunLoop run_loop;
  EXPECT_FALSE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
}

// Tests that StartTimeFetch() returns false if the field trial
// is not configured to allow on-demand time fetches.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWithoutVariationsParam) {
  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_FALSE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileSynced) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced, but the
  // next check should happen soon.
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::Minutes(6), tracker_->GetTimerDelayForTesting());

  field_trial_test_->SetFeatureParams(
      true, 1.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileFeatureDisabled) {
  // Disable network time queries and check that a query is not sent.
  field_trial_test_->SetFeatureParams(
      false, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());

  // Enable time queries and check that a query is sent.
  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerWithDevKeysTest, UpdateFromNetworkBadSignature) {
  SetResponseHandler(base::BindRepeating(&BadSignatureResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadData) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkServerError) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::ServerErrorResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

#if BUILDFLAG(IS_IOS)
// http://crbug.com/658619
#define MAYBE_UpdateFromNetworkNetworkError \
  DISABLED_UpdateFromNetworkNetworkError
#else
#define MAYBE_UpdateFromNetworkNetworkError UpdateFromNetworkNetworkError
#endif
TEST_F(NetworkTimeTrackerTest, MAYBE_UpdateFromNetworkNetworkError) {
  SetResponseHandler(base::BindRepeating(
      &NetworkTimeTrackerTest::NetworkErrorResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkLargeResponse) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;

  tracker_->SetMaxResponseSizeForTesting(3);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->SetMaxResponseSizeForTesting(1024);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerWithDevKeysTest, UpdateFromNetworkFirstSyncPending) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());

  // Do not wait for the fetch to complete; ask for the network time
  // immediately while the request is still pending.
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerWithDevKeysTest,
       UpdateFromNetworkSubsequentSyncPending) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  // After one sync attempt failed, kick off another one, and ask for
  // the network time while it is still pending.
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SUBSEQUENT_SYNC_PENDING,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, CustomFetchBehaviorTest) {
  // On creation, the test is configured as if the feature param is set to
  // FETCHES_IN_BACKGROUND_AND_ON_DEMAND.
  EXPECT_EQ(
      NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      tracker_->GetFetchBehavior());
  // When created with a parameter, the tracker should ignore the feature param,
  // and instead use the parameter.
  Reset(NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_ONLY);
  EXPECT_EQ(NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_ONLY,
            tracker_->GetFetchBehavior());
}

TEST_F(NetworkTimeTrackerTest, UncertaintyHistogram) {
  base::HistogramTester histogram_tester;

  // Verify that the histogram counts are empty initially.
  histogram_tester.ExpectTotalCount("NetworkTime.NetworkTimeUncertainty", 0);

  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());

  // Verify that updating the network time logs the uncertainty correctly.
  histogram_tester.ExpectTotalCount("NetworkTime.NetworkTimeUncertainty", 1);
  histogram_tester.ExpectTimeBucketCount("NetworkTime.NetworkTimeUncertainty",
                                         resolution_ + latency_ + adjustment_,
                                         1);
}

TEST_F(NetworkTimeTrackerTest, ObserverTest) {
  // Test that the observer is notified when the network time changes.
  // Also test that the observer removes itself as an observer when it is
  // destroyed.
  {
    NetworkTimeTestObserver observer(tracker_.get());
    base::Time now = clock_->Now();
    base::TimeTicks now_ticks = tick_clock_->NowTicks();
    base::Time in_network_time = now;
    UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                      now_ticks);
    base::TimeDelta expected_offset = latency_ / 2;

    EXPECT_EQ(observer.times_called_, 1);
    EXPECT_EQ(observer.last_state_.known_time, in_network_time - latency_ / 2);
    EXPECT_EQ(observer.last_state_.system_time, now - expected_offset);
    EXPECT_EQ(observer.last_state_.system_ticks, now_ticks - expected_offset);
    EXPECT_EQ(observer.last_state_.uncertainty,
              resolution_ + latency_ + adjustment_);
  }
  // The observer from the previous scope should have removed itself as an
  // observer when it was destroyed, so this should not crash.
  {
    base::Time now = clock_->Now();
    base::TimeTicks now_ticks = tick_clock_->NowTicks();
    base::Time in_network_time = now;
    UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                      now_ticks);
  }
}

TEST_F(NetworkTimeTrackerTest, OnNetworkTimeTrackerDestroyed) {
  // Reset the clock and tick clock pointers to avoid dangling raw_ptr errors.
  // These clock objects are owned by the NetworkTimeTracker, and the test
  // just keeps an extra pointer to them.
  clock_ = nullptr;
  tick_clock_ = nullptr;

  // The observer should remove itself as an observer when it is notified that
  // its network time tracker is destroyed, so this test should not crash when
  // the observer is destroyed.
  NetworkTimeTestObserver observer(tracker_.get());
  ASSERT_EQ(observer.times_tracker_destroyed_, 0);
  tracker_.reset();
  ASSERT_EQ(observer.times_tracker_destroyed_, 1);
}

}  // namespace network_time
