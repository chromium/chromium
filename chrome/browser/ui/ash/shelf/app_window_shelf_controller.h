// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/shelf_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

class AppWindowShelfItemController;
class ChromeShelfController;
class Profile;

namespace aura {
class Window;
}

namespace wm {
class ActivationClient;
}

class AppWindowShelfController : public wm::ActivationChangeObserver,
                                 public ash::ShelfModelObserver {
 public:
  AppWindowShelfController(const AppWindowShelfController&) = delete;
  AppWindowShelfController& operator=(const AppWindowShelfController&) = delete;

  ~AppWindowShelfController() override;

  // Called by ChromeShelfController when the active user changed and the
  // items need to be updated.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // An additional user identified by |Profile|, got added to the existing
  // session.
  virtual void AdditionalUserAddedToSession(Profile* profile) {}

  // Overriden from client::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  ChromeShelfController* owner() { return owner_; }

 protected:
  explicit AppWindowShelfController(ChromeShelfController* owner);

  virtual AppWindowShelfItemController* ControllerForWindow(
      aura::Window* window) = 0;

  // Called to update local caches when the item |delegate| is replaced. Note,
  // |delegate| might not belong to current shelf controller.
  virtual void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) = 0;

 private:
  // Unowned pointers.
  raw_ptr<ChromeShelfController> owner_;
  raw_ptr<wm::ActivationClient> activation_client_ = nullptr;

  // ash::ShelfModelObserver:
  void ShelfItemDelegateChanged(const ash::ShelfID& id,
                                ash::ShelfItemDelegate* old_delegate,
                                ash::ShelfItemDelegate* delegate) override;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_CONTROLLER_H_
