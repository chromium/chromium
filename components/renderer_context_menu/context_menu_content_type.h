// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_

#include "base/macros.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/models/simple_menu_model.h"

namespace content {
class WebContents;
}

// ContextMenuContentType is a helper to decide which category/group of items
// are relevant for a given WebContents and a context.
//
// Subclasses can override the behavior of showing/hiding a category.
class ContextMenuContentType {
 public:
  virtual ~ContextMenuContentType();

  // Represents a group of menu items.
  // Order matters as they are appended in the enum order.
  enum ItemGroup {
    ITEM_GROUP_CUSTOM,
    ITEM_GROUP_PAGE,
    ITEM_GROUP_FRAME,
    ITEM_GROUP_LINK,
    ITEM_GROUP_SMART_SELECTION,
    ITEM_GROUP_MEDIA_IMAGE,
    ITEM_GROUP_SEARCHWEBFORIMAGE,
    ITEM_GROUP_MEDIA_VIDEO,
    ITEM_GROUP_MEDIA_AUDIO,
    ITEM_GROUP_MEDIA_CANVAS,
    ITEM_GROUP_MEDIA_PLUGIN,
    ITEM_GROUP_MEDIA_FILE,
    ITEM_GROUP_EDITABLE,
    ITEM_GROUP_COPY,
    ITEM_GROUP_SEARCH_PROVIDER,
    ITEM_GROUP_PRINT,
    ITEM_GROUP_ALL_EXTENSION,
    ITEM_GROUP_CURRENT_EXTENSION,
    ITEM_GROUP_DEVELOPER,
    ITEM_GROUP_DEVTOOLS_UNPACKED_EXT,
    ITEM_GROUP_PRINT_PREVIEW,
    ITEM_GROUP_PASSWORD
  };

  // Returns if |group| is enabled.
  virtual bool SupportsGroup(int group);

  ContextMenuContentType(content::WebContents* web_contents,
                         const content::ContextMenuParams& params,
                         bool supports_custom_items);

 protected:
  const content::ContextMenuParams& params() const { return params_; }

  content::WebContents* source_web_contents() const {
    return source_web_contents_;
  }

 private:
  bool SupportsGroupInternal(int group);

  const content::ContextMenuParams params_;
  content::WebContents* const source_web_contents_;
  const bool supports_custom_items_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuContentType);
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_
