// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_

#include <memory>

namespace content {
class WebContents;
}

// Objects implement this interface to get notified about changes in a
// WebContents and to provide necessary functionality.
//
// This is the catch-all interface for holding additions to WebContents that are
// needed when a WebContents is promoted to being a tab contents, but that
// don't cleanly fit elsewhere.
class CoreTabHelperDelegate {
 public:
  // Whether the specified WebContents can be reloaded.
  // Reloading can be disabled e.g. for the DevTools window.
  virtual bool CanReloadContents(content::WebContents* web_contents) const;

  // Whether the specified WebContents can be saved.
  // Saving can be disabled e.g. for the DevTools window.
  virtual bool CanSaveContents(content::WebContents* web_contents) const;

 protected:
  virtual ~CoreTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_
