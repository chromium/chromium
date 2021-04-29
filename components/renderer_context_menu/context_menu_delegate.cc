// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/context_menu_delegate.h"

#include <memory>

#include "content/public/browser/web_contents.h"

namespace {

const char kMenuDelegateUserDataKey[] = "RendererContextMenuMenuDelegate";

class ContextMenuDelegateUserData : public base::SupportsUserData::Data {
 public:
  explicit ContextMenuDelegateUserData(ContextMenuDelegate* menu_delegate)
      : menu_delegate_(menu_delegate) {}
  ~ContextMenuDelegateUserData() override {}
  ContextMenuDelegate* menu_delegate() { return menu_delegate_; }

 private:
  ContextMenuDelegate* menu_delegate_;  // not owned by us.
};

}  // namespace

ContextMenuDelegate::ContextMenuDelegate(content::WebContents* web_contents) {
  web_contents->SetUserData(
      &kMenuDelegateUserDataKey,
      std::make_unique<ContextMenuDelegateUserData>(this));
}

ContextMenuDelegate::~ContextMenuDelegate() {
}

// static
ContextMenuDelegate* ContextMenuDelegate::FromWebContents(
    content::WebContents* web_contents) {
  ContextMenuDelegateUserData* user_data =
      static_cast<ContextMenuDelegateUserData*>(
          web_contents->GetUserData(&kMenuDelegateUserDataKey));
  return user_data ? user_data->menu_delegate() : nullptr;
}
