// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_

#include "base/functional/callback.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace vr {

class VR_EXPORT VrWebContentsObserver : public content::WebContentsObserver {
 public:
  VrWebContentsObserver(content::WebContents* web_contents,
                        base::OnceClosure on_destroy);

  VrWebContentsObserver(const VrWebContentsObserver&) = delete;
  VrWebContentsObserver& operator=(const VrWebContentsObserver&) = delete;

  ~VrWebContentsObserver() override;

 private:
  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  base::OnceClosure on_destroy_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_
