// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

// The cast_shell version of ExtensionWebContentsObserver.
class CastExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<CastExtensionWebContentsObserver> {
 public:
  CastExtensionWebContentsObserver(const CastExtensionWebContentsObserver&) =
      delete;
  CastExtensionWebContentsObserver& operator=(
      const CastExtensionWebContentsObserver&) = delete;

  ~CastExtensionWebContentsObserver() override;

  // Creates and initializes an instance of this class for the given
  // |web_contents|, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<CastExtensionWebContentsObserver>;

  explicit CastExtensionWebContentsObserver(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_WEB_CONTENTS_OBSERVER_H_
