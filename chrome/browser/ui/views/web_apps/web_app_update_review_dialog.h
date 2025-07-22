// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_REVIEW_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_REVIEW_DIALOG_H_

#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"

namespace web_app {

// This property is set on the BrowserView to 'true' when the update dialog is
// showing, and 'false' when it closes.
extern const ui::ClassProperty<bool>* const kIsPwaUpdateDialogShowingKey;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewDialogAcceptButton);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewDialogUninstallButton);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebAppUpdateReviewIgnoreButton);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UPDATE_REVIEW_DIALOG_H_
