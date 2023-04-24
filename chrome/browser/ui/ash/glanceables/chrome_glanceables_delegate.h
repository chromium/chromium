// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_

#include "ash/glanceables/glanceables_delegate.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace ash {
class GlanceablesController;
}  // namespace ash

// Implements the GlanceablesDelegate interface, allowing access to
// functionality in the //chrome/browser layer.
class ChromeGlanceablesDelegate : public ash::GlanceablesDelegate,
                                  public signin::IdentityManager::Observer {
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
  void OnGlanceablesClosed() override {}

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

 private:
  // Returns true if glanceables should be show for the current login.
  bool ShouldShowOnLogin() const;

  const raw_ptr<ash::GlanceablesController, ExperimentalAsh> controller_;

  // The identity manager for the primary profile.
  raw_ptr<signin::IdentityManager, ExperimentalAsh> identity_manager_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
