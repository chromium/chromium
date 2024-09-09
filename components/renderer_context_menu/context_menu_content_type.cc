// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/context_menu_content_type.h"

#include "base/functional/bind.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

using blink::mojom::ContextMenuDataMediaType;
using content::WebContents;

namespace {

bool IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

}  // namespace

ContextMenuContentType::ContextMenuContentType(
    const content::ContextMenuParams& params,
    bool supports_custom_items)
    : params_(params), supports_custom_items_(supports_custom_items) {}

ContextMenuContentType::~ContextMenuContentType() {
}

bool ContextMenuContentType::SupportsGroup(int group) {
  const bool has_selection = !params_.selection_text.empty();

  if (supports_custom_items_ && !params_.custom_items.empty()) {
    if (group == ITEM_GROUP_CUSTOM)
      return true;

    if (!has_selection) {
      // For menus with custom items, if there is no selection, we do not
      // add items other than developer items. And for Pepper menu, don't even
      // add developer items.
      return group == ITEM_GROUP_DEVELOPER;
    }

    // If there's a selection when there are custom items, fall through to
    // adding the normal ones after the custom ones.
  }

  if (IsDevToolsURL(params_.page_url)) {
    // DevTools mostly provides custom context menu and uses
    // only the following default options.
    if (group != ITEM_GROUP_CUSTOM && group != ITEM_GROUP_EDITABLE &&
        group != ITEM_GROUP_COPY && group != ITEM_GROUP_DEVELOPER &&
        group != ITEM_GROUP_SEARCH_PROVIDER) {
      return false;
    }
  }

  return SupportsGroupInternal(group);
}

bool ContextMenuContentType::SupportsGroupInternal(int group) {
  const bool has_link = !params_.unfiltered_link_url.is_empty();
  const bool has_selection = !params_.selection_text.empty();
  const bool is_password = params_.form_control_type ==
                           blink::mojom::FormControlType::kInputPassword;
  const bool existing_highlight = params_.opened_from_highlight;

  switch (group) {
    case ITEM_GROUP_CUSTOM:
      return supports_custom_items_ && !params_.custom_items.empty();

    case ITEM_GROUP_PAGE: {
      bool is_candidate =
          params_.media_type == ContextMenuDataMediaType::kNone && !has_link &&
          !params_.is_editable && !has_selection && !existing_highlight;

      if (!is_candidate && params_.page_url.is_empty())
        DCHECK(params_.frame_url.is_empty());

      return is_candidate && !params_.page_url.is_empty();
    }

    case ITEM_GROUP_FRAME: {
      bool page_group_supported = SupportsGroupInternal(ITEM_GROUP_PAGE);
      return page_group_supported && params_.is_subframe;
    }

    case ITEM_GROUP_LINK:
      return has_link;

    case ITEM_GROUP_SMART_SELECTION:
      return has_selection && !has_link;

    case ITEM_GROUP_MEDIA_IMAGE:
      return params_.media_type == ContextMenuDataMediaType::kImage;

    case ITEM_GROUP_SEARCHWEBFORIMAGE:
      // Image menu items imply search web for image item.
      return SupportsGroupInternal(ITEM_GROUP_MEDIA_IMAGE);

    case ITEM_GROUP_MEDIA_VIDEO:
      return params_.media_type == ContextMenuDataMediaType::kVideo;

    case ITEM_GROUP_MEDIA_AUDIO:
      return params_.media_type == ContextMenuDataMediaType::kAudio;

    case ITEM_GROUP_MEDIA_CANVAS:
      return params_.media_type == ContextMenuDataMediaType::kCanvas;

    case ITEM_GROUP_MEDIA_PLUGIN:
      return params_.media_type == ContextMenuDataMediaType::kPlugin;

    case ITEM_GROUP_MEDIA_FILE:
      return params_.media_type == ContextMenuDataMediaType::kFile;

    case ITEM_GROUP_EDITABLE:
      return params_.is_editable;

    case ITEM_GROUP_COPY:
      return !params_.is_editable && has_selection;

    case ITEM_GROUP_PARTIAL_TRANSLATE:
      return has_selection;

    case ITEM_GROUP_EXISTING_LINK_TO_TEXT:
      return params_.opened_from_highlight;

    case ITEM_GROUP_SEARCH_PROVIDER:
      return has_selection && !is_password;

    case ITEM_GROUP_PRINT: {
      // Image menu items also imply print items.
      return has_selection || SupportsGroupInternal(ITEM_GROUP_MEDIA_IMAGE);
    }

    case ITEM_GROUP_ALL_EXTENSION:
      return true;

    case ITEM_GROUP_CURRENT_EXTENSION:
      return false;

    case ITEM_GROUP_DEVELOPER:
      return true;

    case ITEM_GROUP_DEVTOOLS_UNPACKED_EXT:
      return false;

    case ITEM_GROUP_PRINT_PREVIEW:
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      return true;
#else
      return false;
#endif

    case ITEM_GROUP_AUTOFILL:
      return params_.form_control_type.has_value();

    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}
