// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::babelorca {

class ResponseCallbackWrapper;

struct RequestDataWrapper {
  RequestDataWrapper(
      const net::NetworkTrafficAnnotationTag& annotation_tag_param,
      std::string_view url_param,
      int max_retries_param,
      std::unique_ptr<ResponseCallbackWrapper> response_cb_param);

  ~RequestDataWrapper();

  raw_ref<const net::NetworkTrafficAnnotationTag> annotation_tag;
  const std::string_view url;
  const int max_retries;
  std::unique_ptr<ResponseCallbackWrapper> response_cb;
  int oauth_version = 0;
  int oauth_retry_num = 0;
  std::string content_data;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_REQUEST_DATA_WRAPPER_H_
