// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_TEST_REQUEST_VIEW_HANDLE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_TEST_REQUEST_VIEW_HANDLE_H_

#include <string>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"

namespace dom_distiller {

// TestRequestViewHandle allows the javascript buffer to be collected at any
// point and viewed. This class is for testing only.
class TestRequestViewHandle : public DomDistillerRequestViewBase {
 public:
  explicit TestRequestViewHandle(DistilledPagePrefs* prefs);
  ~TestRequestViewHandle() override;

  std::string GetJavaScriptBuffer();
  void ClearJavaScriptBuffer();

 private:
  void SendJavaScript(const std::string& buffer) override;
  std::string buffer_;
};

TestRequestViewHandle::TestRequestViewHandle(DistilledPagePrefs* prefs)
    : DomDistillerRequestViewBase(prefs) {}

TestRequestViewHandle::~TestRequestViewHandle() {
  distilled_page_prefs_->RemoveObserver(this);
}

std::string TestRequestViewHandle::GetJavaScriptBuffer() {
  return buffer_;
}

void TestRequestViewHandle::ClearJavaScriptBuffer() {
  buffer_ = "";
}

void TestRequestViewHandle::SendJavaScript(const std::string& buffer) {
  buffer_ += buffer;
}

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_TEST_REQUEST_VIEW_HANDLE_H_
