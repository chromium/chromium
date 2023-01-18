// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_web_contents_observer.h"

namespace vr {

VrWebContentsObserver::VrWebContentsObserver(content::WebContents* web_contents,
                                             base::OnceClosure on_destroy)
    : WebContentsObserver(web_contents), on_destroy_(std::move(on_destroy)) {}

VrWebContentsObserver::~VrWebContentsObserver() {}

void VrWebContentsObserver::WebContentsDestroyed() {
  DCHECK(on_destroy_);
  std::move(on_destroy_).Run();
}

}  // namespace vr
