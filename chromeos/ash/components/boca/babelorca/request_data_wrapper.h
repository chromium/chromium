// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::babelorca {

class TachyonResponse;

struct RequestDataWrapper {
  using ResponseCallback = base::OnceCallback<void(TachyonResponse)>;

  RequestDataWrapper(
      const net::NetworkTrafficAnnotationTag& annotation_tag_param,
      std::string_view url_param,
      int max_retries_param,
      ResponseCallback response_cb_param);

  ~RequestDataWrapper();

  const net::NetworkTrafficAnnotationTag annotation_tag;
  const std::string_view url;
  const int max_retries;
  ResponseCallback response_cb;
  int oauth_version = 0;
  int oauth_retry_num = 0;
  std::string content_data;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_
