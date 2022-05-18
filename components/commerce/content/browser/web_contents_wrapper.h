// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "components/commerce/core/web_wrapper.h"
#include "content/public/browser/web_contents.h"

class GURL;

namespace commerce {

// A WebWrapper backed by content::WebContents.
class WebContentsWrapper : public WebWrapper {
 public:
  explicit WebContentsWrapper(content::WebContents* web_contents);
  WebContentsWrapper(const WebContentsWrapper&) = delete;
  WebContentsWrapper operator=(const WebContentsWrapper&) = delete;
  ~WebContentsWrapper() override = default;

  const GURL& GetLastCommittedURL() override;

  bool IsOffTheRecord() override;

  void ClearWebContentsPointer();

 private:
  base::raw_ptr<content::WebContents> web_contents_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_
