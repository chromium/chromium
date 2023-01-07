// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_WORKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_WORKER_H_

namespace autofill_assistant {

// Superclass for workers of |WebController| that execute complex operations.
// This superclass merely ensures that workers can be memory-managed by
// others (in particular an instance of |WebController|), by making the
// base constructor/destructor public.
class WebControllerWorker {
 public:
  WebControllerWorker() = default;

  WebControllerWorker(const WebControllerWorker&) = delete;
  WebControllerWorker& operator=(const WebControllerWorker&) = delete;

  virtual ~WebControllerWorker() = default;
};

}  //  namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_WORKER_H_