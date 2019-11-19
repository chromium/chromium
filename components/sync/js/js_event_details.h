// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_JS_JS_EVENT_DETAILS_H_
#define COMPONENTS_SYNC_JS_JS_EVENT_DETAILS_H_

// See README for design comments.

#include <string>

#include "base/values.h"
#include "components/sync/base/immutable.h"

namespace syncer {

// A thin wrapper around Immutable<DictionaryValue>.  Used for passing
// around event details to different threads.
class JsEventDetails {
 public:
  // Uses an empty dictionary.
  JsEventDetails();

  // Takes over the data in |details|, leaving |details| empty.
  explicit JsEventDetails(base::DictionaryValue* details);

  JsEventDetails(const JsEventDetails& other);

  ~JsEventDetails();

  const base::DictionaryValue& Get() const;

  std::string ToString() const;

  // Copy constructor and assignment operator welcome.

 private:
  using ImmutableDictionaryValue =
      Immutable<base::DictionaryValue,
                HasSwapMemFnByPtr<base::DictionaryValue>>;

  ImmutableDictionaryValue details_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_JS_JS_EVENT_DETAILS_H_
