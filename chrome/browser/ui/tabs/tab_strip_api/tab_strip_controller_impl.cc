// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_controller_impl.h"

#include "base/types/expected.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "mojo/public/mojom/base/error.mojom.h"

TabStripControllerImpl::TabStripControllerImpl(BrowserWindowInterface* browser,
                                               TabStripModel* tab_strip_model)
    : browser_(browser), model_(tab_strip_model) {}

TabStripControllerImpl::~TabStripControllerImpl() = default;

void TabStripControllerImpl::CreateNewTab(CreateNewTabCallback callback) {
  // TODO (crbug.com/411134070) Implement CreateNewTab without using
  // TabStripModel's delegate. The delegate is intended to be a private
  // abstraction used by TabStripModel to interact with the Browser. The correct
  // method to create a new tab should be chrome::AddTabAt. Currently
  // chrome::AddTabAt requires a Browser pointer which is not available with
  // BrowserWindowFeatures.
  // Currently unimplemented, so return the appropriate error code.
  std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
      mojo_base::mojom::Code::kUnimplemented, "unimplemented")));
}
