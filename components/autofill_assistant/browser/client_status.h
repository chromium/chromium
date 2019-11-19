// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_STATUS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_STATUS_H_

#include <memory>

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ClientStatus {
 public:
  ClientStatus();
  explicit ClientStatus(ProcessedActionStatusProto status);
  ~ClientStatus();

  // Returns true if this is an OK status.
  bool ok() const { return status_ == ACTION_APPLIED; }

  // Fills a ProcessedActionProto as appropriate for the current status.
  void FillProto(ProcessedActionProto* proto) const;

  // Returns the corresponding proto status.
  ProcessedActionStatusProto proto_status() const { return status_; }

  // Modifies the corresponding proto status.
  void set_proto_status(ProcessedActionStatusProto status) { status_ = status; }

  // Returns a mutable version of status details, creates one if necessary.
  ProcessedActionStatusDetailsProto* mutable_details() {
    has_details_ = true;
    return &details_;
  }

  // Returns the status details associated with this status.
  const ProcessedActionStatusDetailsProto& details() const { return details_; }

  // The output operator, for logging.
  friend std::ostream& operator<<(std::ostream& out,
                                  const ClientStatus& status);

 private:
  ProcessedActionStatusProto status_;
  bool has_details_ = false;
  ProcessedActionStatusDetailsProto details_;
};

// An OK status.
const ClientStatus& OkClientStatus();

// Intended for debugging and test error output. Writes a string representation
// of the status to |out|.
std::ostream& operator<<(std::ostream& out,
                         const ProcessedActionStatusProto& status);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_STATUS_H_
