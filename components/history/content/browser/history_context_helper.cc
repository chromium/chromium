// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/history_context_helper.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {

// WebContentsContext is used to bind an history::Context to a
// content::WebContents.
class WebContentsContext
    : public history::Context,
      public content::WebContentsUserData<WebContentsContext> {
 private:
  friend class content::WebContentsUserData<WebContentsContext>;
  explicit WebContentsContext(content::WebContents* web_contents)
      : content::WebContentsUserData<WebContentsContext>(*web_contents) {}
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsContext);

}  // namespace

namespace history {

ContextID ContextIDForWebContents(content::WebContents* web_contents) {
  WebContentsContext::CreateForWebContents(web_contents);
  return WebContentsContext::FromWebContents(web_contents)->GetContextID();
}

}  // namespace history
