// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"

#include <string>
#include <string_view>
#include <utility>

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::babelorca {

RequestDataWrapper::RequestDataWrapper(
    const net::NetworkTrafficAnnotationTag& annotation_tag_param,
    std::string_view url_param,
    int max_retries_param,
    ResponseCallback response_cb_param)
    : annotation_tag(annotation_tag_param),
      url(std::move(url_param)),
      max_retries(max_retries_param),
      response_cb(std::move(response_cb_param)) {}

RequestDataWrapper::~RequestDataWrapper() = default;

}  // namespace ash::babelorca
