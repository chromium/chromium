// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/clients.h"

namespace download {

// Must sync variants "DownloadClient" in histograms.xml
std::string BackgroundDownloadClientToString(DownloadClient client) {
  switch (client) {
    case DownloadClient::TEST:
    case DownloadClient::TEST_2:
    case DownloadClient::TEST_3:
    case DownloadClient::INVALID:
      return "__Test__";
    case DownloadClient::OFFLINE_PAGE_PREFETCH:
      return "OfflinePage";
    case DownloadClient::BACKGROUND_FETCH:
      return "BackgroundFetch";
    case DownloadClient::DEBUGGING:
      return "Debugging";
    case DownloadClient::MOUNTAIN_INTERNAL:
      return "MountainInternal";
    case DownloadClient::PLUGIN_VM_IMAGE:
      return "PluginVmImage";
    case DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS:
      return "OptimizationGuidePredictionModels";
    case DownloadClient::BRUSCHETTA:
      return "Bruschetta";
    case DownloadClient::BOUNDARY:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace download
