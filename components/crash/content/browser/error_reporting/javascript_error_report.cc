// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/error_reporting/javascript_error_report.h"

JavaScriptErrorReport::JavaScriptErrorReport() = default;
JavaScriptErrorReport::JavaScriptErrorReport(const JavaScriptErrorReport& rhs) =
    default;
JavaScriptErrorReport::JavaScriptErrorReport(
    JavaScriptErrorReport&& rhs) noexcept = default;
JavaScriptErrorReport::~JavaScriptErrorReport() = default;
JavaScriptErrorReport& JavaScriptErrorReport::operator=(
    const JavaScriptErrorReport& rhs) = default;
JavaScriptErrorReport& JavaScriptErrorReport::operator=(
    JavaScriptErrorReport&& rhs) noexcept = default;
