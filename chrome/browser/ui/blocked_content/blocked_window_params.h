// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_

#include "content/public/common/referrer.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

struct NavigateParams;

namespace content {
class RenderProcessHost;
class SiteInstance;
class WebContents;
}  // namespace content

class BlockedWindowParams {
 public:
  BlockedWindowParams(const GURL& target_url,
                      const url::Origin& initiator_origin,
                      content::SiteInstance* source_site_instance,
                      const content::Referrer& referrer,
                      const std::string& frame_name_,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& features,
                      bool user_gesture,
                      bool opener_suppressed);
  BlockedWindowParams(const BlockedWindowParams& other);
  ~BlockedWindowParams();

  NavigateParams CreateNavigateParams(
      content::RenderProcessHost* opener_process,
      content::WebContents* web_contents) const;

  blink::mojom::WindowFeatures features() const { return features_; }

 private:
  GURL target_url_;
  url::Origin initiator_origin_;
  scoped_refptr<content::SiteInstance> source_site_instance_;
  content::Referrer referrer_;
  std::string frame_name_;
  WindowOpenDisposition disposition_;
  blink::mojom::WindowFeatures features_;
  bool user_gesture_;
  bool opener_suppressed_;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_
