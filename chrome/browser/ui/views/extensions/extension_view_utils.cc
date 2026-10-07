// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view_utils.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {

class ExtensionsContainerOverrideData
    : public content::WebContentsUserData<ExtensionsContainerOverrideData> {
 public:
  ~ExtensionsContainerOverrideData() override = default;

  ExtensionsContainerViews* container() const { return container_.get(); }
  void set_container(base::WeakPtr<ExtensionsContainerViews> container) {
    container_ = std::move(container);
  }

 private:
  friend class content::WebContentsUserData<ExtensionsContainerOverrideData>;

  explicit ExtensionsContainerOverrideData(content::WebContents* web_contents)
      : content::WebContentsUserData<ExtensionsContainerOverrideData>(
            *web_contents) {}

  base::WeakPtr<ExtensionsContainerViews> container_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExtensionsContainerOverrideData);

}  // namespace

void SetExtensionsContainerViewsForWebContents(
    content::WebContents* web_contents,
    base::WeakPtr<ExtensionsContainerViews> container) {
  if (!web_contents) {
    return;
  }
  if (!container) {
    web_contents->RemoveUserData(
        ExtensionsContainerOverrideData::UserDataKey());
    return;
  }
  ExtensionsContainerOverrideData::CreateForWebContents(web_contents);
  ExtensionsContainerOverrideData::FromWebContents(web_contents)
      ->set_container(std::move(container));
}

ExtensionsContainerViews* GetExtensionsContainerViews(
    gfx::NativeWindow parent) {
  CHECK(parent);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForNativeWindow(parent);

  return browser_view ? browser_view->toolbar_button_provider()
                            ->GetExtensionsContainerViews()
                      : nullptr;
}

ExtensionsContainerViews* GetExtensionsContainerViews(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (auto* override_data =
          ExtensionsContainerOverrideData::FromWebContents(web_contents)) {
    if (ExtensionsContainerViews* container = override_data->container()) {
      return container;
    }
  }
  gfx::NativeWindow parent = web_contents->GetTopLevelNativeWindow();
  return parent ? GetExtensionsContainerViews(parent) : nullptr;
}

// TODO(crbug.com/40839674): Use extensions::IconImage instead of getting the
// action's image. The icon displayed should be the "product" icon and not the
// "action" action based on the web contents.
ui::ImageModel GetIcon(ToolbarActionViewModel* action,
                       content::WebContents* web_contents) {
  return action->GetIcon(web_contents,
                         gfx::Size(extension_misc::EXTENSION_ICON_SMALLISH,
                                   extension_misc::EXTENSION_ICON_SMALLISH));
}
