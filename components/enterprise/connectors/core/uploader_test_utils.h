// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_UPLOADER_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_UPLOADER_TEST_UTILS_H_

#include <string>

namespace enterprise_connectors {

class ConnectorDataPipeGetter;

std::string GetBodyFromFileOrPageRequest(
    ConnectorDataPipeGetter* data_pipe_getter);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_UPLOADER_TEST_UTILS_H_
