// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_OPERATION_TOKEN_H_
#define COMPONENTS_FEED_CORE_V2_OPERATION_TOKEN_H_

#include "base/memory/weak_ptr.h"

namespace feed {

// A copyable object which Tracks whether or not an `Operation` is alive.
class OperationToken {
 public:
  static OperationToken MakeInvalid();
  ~OperationToken();
  OperationToken(const OperationToken& src);
  OperationToken& operator=(const OperationToken& src);

  // Returns whether the operation is alive. Returns `false` if the operation
  // has been destroyed.
  explicit operator bool() const;

  // An operation which can be tracked by `OperationToken`.
  class Operation {
   public:
    Operation();
    ~Operation();
    Operation(const Operation&) = delete;
    Operation& operator=(const Operation&) const = delete;

    // Reset the operation, and start a new one. All existing operation
    // tokens will report this operation as destroyed.
    void Reset();
    // Return a token pointing to this operation.
    OperationToken Token();

   private:
    base::WeakPtrFactory<Operation> weak_ptr_factory_{this};
  };

 private:
  explicit OperationToken(base::WeakPtr<Operation> token);
  base::WeakPtr<Operation> token_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_OPERATION_TOKEN_H_
