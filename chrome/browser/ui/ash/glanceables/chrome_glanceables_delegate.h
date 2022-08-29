// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_

#include "ash/glanceables/glanceables_delegate.h"

class Profile;

namespace ash {
class GlanceablesController;
}  // namespace ash

// Implements the GlanceablesDelegate interface, allowing access to
// functionality in the //chrome/browser layer.
class ChromeGlanceablesDelegate : public ash::GlanceablesDelegate {
 public:
  explicit ChromeGlanceablesDelegate(ash::GlanceablesController* controller);
  ChromeGlanceablesDelegate(const ChromeGlanceablesDelegate&) = delete;
  ChromeGlanceablesDelegate& operator=(const ChromeGlanceablesDelegate&) =
      delete;
  ~ChromeGlanceablesDelegate() override;

  static ChromeGlanceablesDelegate* Get();

  // Called when the primary user logs in, after various KeyedServices are
  // created.
  void OnPrimaryUserSessionStarted(Profile* profile);

  // ash::GlanceablesDelegate:
  void RestoreSession() override;
  void OnGlanceablesClosed() override;

 private:
  // Returns true if glanceables should be show for the current login.
  bool ShouldShowOnLogin() const;

  ash::GlanceablesController* const controller_;

  // The profile for the primary user in the session.
  Profile* primary_profile_ = nullptr;

  // Whether the user triggered session restore on login.
  bool did_restore_ = false;
};

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
