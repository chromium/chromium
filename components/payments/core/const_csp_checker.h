// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_CONST_CSP_CHECKER_H_
#define COMPONENTS_PAYMENTS_CORE_CONST_CSP_CHECKER_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/core/csp_checker.h"

namespace payments {

// A test-only class for either always allowing or always denying connections,
// depending on the input parameter for the constructor.
class ConstCSPChecker : public CSPChecker {
 public:
  explicit ConstCSPChecker(bool allow);
  ~ConstCSPChecker() override;

  ConstCSPChecker(const ConstCSPChecker& other) = delete;
  ConstCSPChecker& operator=(const ConstCSPChecker& other) = delete;

  // CSPChecker:
  void AllowConnectToSource(
      const GURL& url,
      const GURL& url_before_redirects,
      bool did_follow_redirect,
      base::OnceCallback<void(bool)> result_callback) override;

  base::WeakPtr<ConstCSPChecker> GetWeakPtr();

  void InvalidateWeakPtrsForTesting();

 private:
  bool allow_ = false;
  base::WeakPtrFactory<ConstCSPChecker> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_CONST_CSP_CHECKER_H_
