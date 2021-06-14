// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include "chromecast/browser/cast_content_browser_client.h"

namespace chromecast {

class CastFeatureListCreator;

class CastRuntimeContentBrowserClient final
    : public shell::CastContentBrowserClient {
 public:
  static std::unique_ptr<CastRuntimeContentBrowserClient> Create(
      CastFeatureListCreator* feature_list_creator);

  explicit CastRuntimeContentBrowserClient(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClient() override;

  // CastContentBrowserClient overrides:
  std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager) final;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
