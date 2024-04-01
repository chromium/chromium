// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/commerce/core/web_wrapper.h"
#include "content/public/browser/web_contents.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

namespace commerce {

// A WebWrapper backed by content::WebContents.
class WebContentsWrapper : public WebWrapper {
 public:
  explicit WebContentsWrapper(content::WebContents* web_contents,
                              int32_t js_world_id);
  WebContentsWrapper(const WebContentsWrapper&) = delete;
  WebContentsWrapper operator=(const WebContentsWrapper&) = delete;
  ~WebContentsWrapper() override;

  const GURL& GetLastCommittedURL() override;

  const std::u16string& GetTitle() override;

  bool IsFirstLoadForNavigationFinished() override;

  void SetIsFirstLoadForNavigationFinished(bool finished);

  bool IsOffTheRecord() override;

  void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) override;

  ukm::SourceId GetPageUkmSourceId() override;

  void ClearWebContentsPointer();

  content::RenderFrameHost* GetPrimaryMainFrame();

 private:
  raw_ptr<content::WebContents> web_contents_;

  // The ID of the isolated world to run javascript in.
  int32_t js_world_id_;

  // Whether the first load after a navigation has completed. This is useful
  // when dealing with single-page apps that may not fire subsequent load
  // events.
  bool is_first_load_for_nav_finished_{false};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_CONTENTS_WRAPPER_H_
