// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/models/simple_menu_model.h"

// ContextMenuContentType is a helper to decide which category/group of items
// are relevant for a given WebContents and a context.
//
// Subclasses can override the behavior of showing/hiding a category.
class ContextMenuContentType {
 public:
  ContextMenuContentType(const ContextMenuContentType&) = delete;
  ContextMenuContentType& operator=(const ContextMenuContentType&) = delete;

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
    ITEM_GROUP_PARTIAL_TRANSLATE,
    ITEM_GROUP_SEARCH_PROVIDER,
    ITEM_GROUP_PRINT,
    ITEM_GROUP_ALL_EXTENSION,
    ITEM_GROUP_CURRENT_EXTENSION,
    ITEM_GROUP_DEVELOPER,
    ITEM_GROUP_DEVTOOLS_UNPACKED_EXT,
    ITEM_GROUP_PRINT_PREVIEW,
    ITEM_GROUP_EXISTING_LINK_TO_TEXT,
    ITEM_GROUP_AUTOFILL
  };

  // Returns if |group| is enabled.
  virtual bool SupportsGroup(int group);

  ContextMenuContentType(const content::ContextMenuParams& params,
                         bool supports_custom_items);

 protected:
  const content::ContextMenuParams& params() const { return params_; }

 private:
  bool SupportsGroupInternal(int group);

  const content::ContextMenuParams params_;
  const bool supports_custom_items_;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_H_
