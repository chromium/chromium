// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_ENTITY_SUPPRESSION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_ENTITY_SUPPRESSION_DIALOG_VIEW_H_

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// The name of the dialog's widget.
inline constexpr char kEntitySuppressionDialogName[] =
    "EntitySuppressionDialog";

// Shows a tab-modal dialog that asks the user to confirm that an Autofill AI
// entity will no longer be suggested. `num_sources` is the number of sources
// the entity was extracted from; it determines whether the dialog talks about
// the user's original source or sources. `callback` is run with true if the
// user accepts, and with false if they cancel or close the dialog.
void ShowEntitySuppressionDialogView(content::WebContents* web_contents,
                                     int num_sources,
                                     base::OnceCallback<void(bool)> callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_ENTITY_SUPPRESSION_DIALOG_VIEW_H_
