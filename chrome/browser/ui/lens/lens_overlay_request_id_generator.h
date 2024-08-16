// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_

#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"

namespace lens {

// Manages creating lens overlay request IDs. Owned by a single Lens overlay
// query controller.
class LensOverlayRequestIdGenerator {
 public:
  LensOverlayRequestIdGenerator();
  ~LensOverlayRequestIdGenerator();

  // Resets the request id generator, creating a new uuid and resetting the
  // sequence.
  void ResetRequestId();

  // Creates a new analytics id to use in followup requests. This should be
  // called for each interaction.
  void CreateNewAnalyticsId();

  // Increments the sequence and returns the next request id.
  std::unique_ptr<lens::LensOverlayRequestId> GetNextRequestId();

  // Returns the current analytics id as a base32 encoded string.
  std::string GetBase32EncodedAnalyticsId();

 private:
  // The current uuid. Valid for the duration of a Lens overlay session.
  uint64_t uuid_;

  // The analytics id for the current request. Will be updated on each
  // query.
  std::string analytics_id_;

  // The current sequence id.
  int sequence_id_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
