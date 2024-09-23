// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of url/url_canon_stdstring.h circa 2023. It should be used
// only by components/feedback/redaction_tool/. We need a copy because the
// components/feedback/redaction_tool source code is shared into ChromeOS and
// needs to have no dependencies outside of base/.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_STDSTRING_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_STDSTRING_H_

// This header file defines a canonicalizer output method class for STL
// strings. Because the canonicalizer tries not to be dependent on the STL,
// we have segregated it here.

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/feedback/redaction_tool/url_canon.h"

namespace redaction_internal {

// Write into a std::string given in the constructor. This object does not own
// the string itself, and the user must ensure that the string stays alive
// throughout the lifetime of this object.
//
// The given string will be appended to; any existing data in the string will
// be preserved.
//
// Note that when canonicalization is complete, the string will likely have
// unused space at the end because we make the string very big to start out
// with (by |initial_size|). This ends up being important because resize
// operations are slow, and because the base class needs to write directly
// into the buffer.
//
// Therefore, the user should call Complete() before using the string that
// this class wrote into.
class StdStringCanonOutput : public CanonOutput {
 public:
  explicit StdStringCanonOutput(std::string* str);

  StdStringCanonOutput(const StdStringCanonOutput&) = delete;
  StdStringCanonOutput& operator=(const StdStringCanonOutput&) = delete;

  ~StdStringCanonOutput() override;

  // Must be called after writing has completed but before the string is used.
  void Complete();

  void Resize(size_t sz) override;

 protected:
  // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
  // profiler data and tab_search:top100:2020.
  RAW_PTR_EXCLUSION std::string* str_;
};

}  // namespace redaction_internal

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_URL_CANON_STDSTRING_H_
