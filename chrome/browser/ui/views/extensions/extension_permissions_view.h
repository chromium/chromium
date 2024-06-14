// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_

#include <vector>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace extensions {
struct InstallPromptPermissions;
}  // namespace extensions

// A custom view for the permissions section of the extension info. It contains
// the labels for each permission and the views for their associated details, if
// there are any.
class ExtensionPermissionsView : public views::View {
  METADATA_HEADER(ExtensionPermissionsView, views::View)

 public:
  ExtensionPermissionsView();
  ExtensionPermissionsView(const ExtensionPermissionsView&) = delete;
  ExtensionPermissionsView& operator=(const ExtensionPermissionsView&) = delete;

  // Adds a single pair of |permission_text| and |permission_details| to
  // be rendered in the view.
  void AddItem(const std::u16string& permission_text,
               const std::u16string& permission_details);

  // Adds the set of |permissions| to be rendered in the view.
  void AddPermissions(const extensions::InstallPromptPermissions& permissions);

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_PERMISSIONS_VIEW_H_
