// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_

#include <memory>

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

namespace chromecast {
namespace shell {

// A client that creates Starboard-based CDM factories. These factories produce
// CDMs that allow decryption to be done in Starboard.
//
// Aside from this, this client is identical to CastRuntimeBrowserClient.
class CastRuntimeContentBrowserClientStarboard
    : public CastRuntimeContentBrowserClient {
 public:
  explicit CastRuntimeContentBrowserClientStarboard(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClientStarboard() override;

  // CastRuntimeContentBrowserClient implementation:
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_STARBOARD_H_
