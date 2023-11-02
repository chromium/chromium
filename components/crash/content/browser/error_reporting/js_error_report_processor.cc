// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"

#include "base/no_destructor.h"

namespace {
scoped_refptr<JsErrorReportProcessor>& GetPointer() {
  static base::NoDestructor<scoped_refptr<JsErrorReportProcessor>>
      default_processor;
  return *default_processor;
}
}  // namespace

JsErrorReportProcessor::JsErrorReportProcessor() = default;
JsErrorReportProcessor::~JsErrorReportProcessor() = default;

// static
scoped_refptr<JsErrorReportProcessor> JsErrorReportProcessor::Get() {
  return GetPointer();
}

// static
void JsErrorReportProcessor::SetDefault(
    scoped_refptr<JsErrorReportProcessor> processor) {
  GetPointer() = processor;
}
