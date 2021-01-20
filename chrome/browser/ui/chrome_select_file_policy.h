// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_
#define CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}

// A chrome specific SelectFilePolicy that checks local_state(), and will
// display an infobar on the weakly owned |source_contents|.
class ChromeSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  explicit ChromeSelectFilePolicy(content::WebContents* source_contents);
  ~ChromeSelectFilePolicy() override;

  // Overridden from ui::SelectFilePolicy:
  bool CanOpenSelectFileDialog() override;
  void SelectFileDenied() override;

  // Returns true if local state allows showing file pickers.
  static bool FileSelectDialogsAllowed();

 private:
  content::WebContents* source_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSelectFilePolicy);
};

#endif  // CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_
