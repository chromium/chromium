// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

using content::WebContents;

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    WebContents* web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    const absl::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr,
    const std::u16string& program_name) {
  navigation_interception::InterceptNavigationDelegate* delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (!delegate)
    return;

  delegate->HandleExternalProtocolDialog(url, page_transition, has_user_gesture,
                                         initiating_origin);
}
