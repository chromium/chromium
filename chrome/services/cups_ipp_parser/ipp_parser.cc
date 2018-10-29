// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/ipp_parser.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chrome {

IppParser::IppParser(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

IppParser::~IppParser() = default;

}  // namespace chrome
