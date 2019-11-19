// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_UI_MAIN_DIALOG_API_H_
#define CHROME_CHROME_CLEANER_UI_MAIN_DIALOG_API_H_

#include <windows.h>

#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// A set of callbacks for either the main controller or test code.
class MainDialogDelegate {
 public:
  virtual ~MainDialogDelegate() = default;

  // Identifies whether the user accepted the cleanup (|confirmed| is true).
  virtual void AcceptedCleanup(bool confirmed) = 0;

  // Called when the main dialog is closed.
  virtual void OnClose() = 0;
};

// An abstract base class to be used as an API for the MainDialog, which can
// be replaced for testing purposes.
class MainDialogAPI {
 public:
  explicit MainDialogAPI(MainDialogDelegate* delegate) : delegate_(delegate) {}
  virtual ~MainDialogAPI() {}

  // Create the dialog. This must be called before any of the other methods.
  virtual bool Create() = 0;

  // Show the "No PUPs found" message.
  virtual void NoPUPsFound() = 0;

  // Set the dialog to the "done cleanup" state. The message to be displayed
  // depends on the value of |cleanup_result|.
  virtual void CleanupDone(ResultCode cleanup_result) = 0;

  // Close the window.
  virtual void Close() = 0;

  // Disables |extensions| by telling Chrome to do so.
  // Calls the |on_disable| with the result on completion.
  virtual void DisableExtensions(const std::vector<base::string16>& extensions,
                                 base::OnceCallback<void(bool)> on_disable) = 0;

  // Checks if |found_pups| contains any files to clean. If so, calls
  // ConfirmCleanupWithFiles, otherwise calls NoPUPsFound.
  void ConfirmCleanupIfNeeded(const std::vector<UwSId>& found_pups,
                              scoped_refptr<DigestVerifier> digest_verifier);

 protected:
  // Ask the user to confirm the cleanup of the PUPs in |found_pups|, which
  // will involve removing the files in |files_to_remove| and cleaning registry
  // keys in |registry_keys|. This is only the list of items reported to the
  // user; it doesn't affect the items that will actually be cleaned.
  virtual void ConfirmCleanup(
      const std::vector<UwSId>& found_pups,
      const FilePathSet& files_to_remove,
      const std::vector<base::string16>& registry_keys) = 0;

  MainDialogDelegate* delegate() { return delegate_; }

 private:
  MainDialogDelegate* delegate_;  // Weak.
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_UI_MAIN_DIALOG_API_H_
