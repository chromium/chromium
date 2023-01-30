// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of url/url_canon_stdstring.cc circa 2023. It should be used
// only by components/feedback/redaction_tool/. We need a copy because the
// components/feedback/redaction_tool source code is shared into ChromeOS and
// needs to have no dependencies outside of base/.

#include "components/feedback/redaction_tool/url_canon_stdstring.h"

namespace redaction_internal {

StdStringCanonOutput::StdStringCanonOutput(std::string* str) : str_(str) {
  cur_len_ = str_->size();  // Append to existing data.
  buffer_ = str_->empty() ? nullptr : &(*str_)[0];
  buffer_len_ = str_->size();
}

StdStringCanonOutput::~StdStringCanonOutput() = default;

void StdStringCanonOutput::Complete() {
  str_->resize(cur_len_);
  buffer_len_ = cur_len_;
}

void StdStringCanonOutput::Resize(size_t sz) {
  str_->resize(sz);
  buffer_ = str_->empty() ? nullptr : &(*str_)[0];
  buffer_len_ = sz;
}

}  // namespace redaction_internal
