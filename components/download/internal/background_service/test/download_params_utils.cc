// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/uuid.h"
#include "components/download/internal/background_service/test/download_params_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
namespace download {
namespace test {

DownloadParams BuildBasicDownloadParams() {
  DownloadParams params;
  params.client = DownloadClient::TEST;
  params.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  return params;
}

}  // namespace test
}  // namespace download
