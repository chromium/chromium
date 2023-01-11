// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/const_csp_checker.h"

#include "base/functional/callback.h"

namespace payments {

ConstCSPChecker::ConstCSPChecker(bool allow) : allow_(allow) {}

ConstCSPChecker::~ConstCSPChecker() = default;

void ConstCSPChecker::AllowConnectToSource(
    const GURL& url,
    const GURL& url_before_redirects,
    bool did_follow_redirect,
    base::OnceCallback<void(bool)> result_callback) {
  std::move(result_callback).Run(allow_);
}

base::WeakPtr<ConstCSPChecker> ConstCSPChecker::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ConstCSPChecker::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace payments
