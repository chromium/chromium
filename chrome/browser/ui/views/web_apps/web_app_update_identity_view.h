// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_IDENTITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_IDENTITY_VIEW_H_

#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace web_app {

// This view displays an app identity in a column layout, where the logo, name,
// and origin are shown in 3 rows. This view is used in the update dialog.
class WebAppUpdateIdentityView : public views::View {
  METADATA_HEADER(WebAppUpdateIdentityView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNameLabelId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIconLabelId);

  explicit WebAppUpdateIdentityView(const WebAppIdentity& identity);

  WebAppUpdateIdentityView(const WebAppUpdateIdentityView&) = delete;
  WebAppUpdateIdentityView& operator=(const WebAppUpdateIdentityView&) = delete;
  ~WebAppUpdateIdentityView() override;
};

BEGIN_VIEW_BUILDER(/* no export */, WebAppUpdateIdentityView, views::View)
END_VIEW_BUILDER

}  // namespace web_app

DEFINE_VIEW_BUILDER(/* no export */, web_app::WebAppUpdateIdentityView)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_IDENTITY_VIEW_H_
