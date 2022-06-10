// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_BASE_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_BASE_ELEMENT_FINDER_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class ClientStatus;
class ElementFinderResult;

class BaseElementFinder {
 public:
  using Callback =
      base::OnceCallback<void(const ClientStatus&,
                              std::unique_ptr<ElementFinderResult>)>;

  virtual ~BaseElementFinder();

  // Start looking for the element and return it through |callback| with
  // a status. If |start_element| is not empty, use it as a starting point
  // instead of starting from the main frame.
  virtual void Start(const ElementFinderResult& start_element,
                     Callback callback) = 0;

  // Get the log information for the last run. Should only be run after the
  // run has completed (i.e. |callback_| has been called).
  virtual ElementFinderInfoProto GetLogInfo() const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_BASE_ELEMENT_FINDER_H_
