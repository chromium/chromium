// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_

#include "base/functional/callback_forward.h"

class GURL;

namespace autofill {

// Shows the source document of an Autofill AI verification next to the tab
// with the filled form.
class AutofillAiSourceSplitView {
 public:
  virtual ~AutofillAiSourceSplitView() = default;

  // Returns true if `url` can be shown next to the form.
  virtual bool CanOpen(const GURL& url) const = 0;

  // Opens `url` next to the form. Returns false if that failed. `on_closed`
  // runs at most once, when the user closes the source; it does not run if
  // opening failed or after `Close()`.
  virtual bool Open(const GURL& url, base::OnceClosure on_closed) = 0;

  // Closes the source and restores what was shown before `Open()`. Does not
  // run `on_closed`. Does nothing if the source is not open.
  virtual void Close() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_
