// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

namespace content {
class WebContents;
}  // namespace content

class ExtensionsMenuViewModel;

class ExtensionsMenuViewPlatformDelegate {
 public:
  virtual ~ExtensionsMenuViewPlatformDelegate() = default;

  // Attaches the delegate to a platform-agnostic menu view model. It is called
  // by the model on its constructor.
  virtual void AttachToModel(ExtensionsMenuViewModel* model) = 0;

  // Detaches the delegate from a platform-agnostic menu view model. It is
  // called by the model on its destructor.
  virtual void DetachFromModel() = 0;

  // Notifies the delegate that a new host access request was added for
  // `extension_id` on `web_contents`.
  // TODO(crbug.com/449814184): Rename to `OnHostAccessRequestAdded` after we
  // finish migrating all PermissionsManager::Observer method from the platform
  // delegate to the model, since same name causes parameter type mismatch.
  virtual void OnAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                    content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_
