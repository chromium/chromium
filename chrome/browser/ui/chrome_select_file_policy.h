// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_
#define CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_

#include "base/memory/raw_ptr.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}

// A chrome specific SelectFilePolicy that checks local_state(), and will
// display an infobar on the weakly owned |source_contents|.
class ChromeSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  explicit ChromeSelectFilePolicy(content::WebContents* source_contents);

  ChromeSelectFilePolicy(const ChromeSelectFilePolicy&) = delete;
  ChromeSelectFilePolicy& operator=(const ChromeSelectFilePolicy&) = delete;

  ~ChromeSelectFilePolicy() override;

  // Overridden from ui::SelectFilePolicy:
  bool CanOpenSelectFileDialog() override;
  void SelectFileDenied() override;

  // Returns true if local state allows showing file pickers.
  static bool FileSelectDialogsAllowed();

 private:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> source_contents_;
};

#endif  // CHROME_BROWSER_UI_CHROME_SELECT_FILE_POLICY_H_
