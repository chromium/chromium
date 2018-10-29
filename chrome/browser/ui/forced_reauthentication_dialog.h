// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FORCED_REAUTHENTICATION_DIALOG_H_
#define CHROME_BROWSER_UI_FORCED_REAUTHENTICATION_DIALOG_H_

#include <memory>

#include "base/macros.h"

namespace identity {
class IdentityManager;
}

class Profile;

namespace base {
class TimeDelta;
}  // namespace base

// The virtual class of ForcedReauthenticationDialog.
class ForcedReauthenticationDialog {
 public:
  static std::unique_ptr<ForcedReauthenticationDialog> Create();

  virtual ~ForcedReauthenticationDialog() {}
  // Show the ForcedReauthenticationDialog for |profile|. If there're no opened
  // browser windows for |profile|, |identity_manager| will be called to signed
  // out immediately. Otherwise, dialog will be closed with all browser windows
  // are associated to |profile| after |countdown_duration| if there is no
  // reauth.
  virtual void ShowDialog(Profile* profile,
                          identity::IdentityManager* identity_manager,
                          base::TimeDelta countdown_duration) = 0;

 protected:
  ForcedReauthenticationDialog() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ForcedReauthenticationDialog);
};

#endif  // CHROME_BROWSER_UI_FORCED_REAUTHENTICATION_DIALOG_H_
