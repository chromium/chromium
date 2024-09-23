// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_PLATFORM_SPECIFIC_TAB_DATA_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_PLATFORM_SPECIFIC_TAB_DATA_H_

#include "base/memory/ref_counted.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/sessions_export.h"
#include "content/public/browser/session_storage_namespace.h"

namespace content {
class WebContents;
}

namespace sessions {

// A //content-specific subclass of PlatformSpecificTabData that is used to
// associate tab_restore::Tab instances with the
// content::SessionStorageNamespace of the WebContents from which they were
// created.
class SESSIONS_EXPORT ContentPlatformSpecificTabData
    : public tab_restore::PlatformSpecificTabData {
 public:
  explicit ContentPlatformSpecificTabData(content::WebContents* web_contents);
  ContentPlatformSpecificTabData();
  ~ContentPlatformSpecificTabData() override;

  content::SessionStorageNamespace* session_storage_namespace() const {
    return session_storage_namespace_.get();
  }

 private:
  scoped_refptr<content::SessionStorageNamespace> session_storage_namespace_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_PLATFORM_SPECIFIC_TAB_DATA_H_
