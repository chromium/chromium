// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_VIEW_UTILS_H_

#include "ui/base/models/dialog_model.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// Adds a sign-in promo footnote to the dialog model builder if necessary.
// This function is specific to the Views implementation.
void MaybeAddSigninPromoFootnoteView(
    Profile* profile,
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    ui::DialogModel::Builder& dialog_model_builder);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_VIEW_UTILS_H_
