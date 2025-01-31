// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"

namespace autofill {

BnplTosControllerImpl::BnplTosControllerImpl() = default;

BnplTosControllerImpl::~BnplTosControllerImpl() = default;

void BnplTosControllerImpl::OnViewClosing() {
  // The view is being closed so set the pointer to nullptr.
  view_.reset();
}

base::WeakPtr<BnplTosController> BnplTosControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BnplTosControllerImpl::Show(
    base::OnceCallback<std::unique_ptr<BnplTosView>()>
        create_and_show_view_callback) {
  // If the view already exists, don't create and show a new view.
  if (view_) {
    return;
  }
  view_ = std::move(create_and_show_view_callback).Run();
}

}  // namespace autofill
