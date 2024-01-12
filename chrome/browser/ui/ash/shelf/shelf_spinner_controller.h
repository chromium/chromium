// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_SHELF_SPINNER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_SHELF_SPINNER_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/shelf_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

class ShelfSpinnerItemController;
class ChromeShelfController;
class Profile;

namespace ash {
class ShelfItemDelegate;
}  // namespace ash

namespace gfx {
class ImageSkia;
}  // namespace gfx

// ShelfSpinnerController displays visual feedback that the application the user
// has just activated will not be immediately available, as it is for example
// waiting for ARC or Crostini to be ready.
class ShelfSpinnerController : public ash::ShelfModelObserver {
 public:
  // ShelfSpinnerData holds the information used to draw the spinner, including
  // animating the spinner after it has been dismissed.
  class ShelfSpinnerData;

  explicit ShelfSpinnerController(ChromeShelfController* owner);

  ShelfSpinnerController(const ShelfSpinnerController&) = delete;
  ShelfSpinnerController& operator=(const ShelfSpinnerController&) = delete;

  ~ShelfSpinnerController() override;

  bool HasApp(const std::string& app_id) const;

  base::TimeDelta GetActiveTime(const std::string& app_id) const;

  // Adds a spinner to the shelf unless the app is already running.
  void AddSpinnerToShelf(
      const std::string& app_id,
      std::unique_ptr<ShelfSpinnerItemController> controller);

  // Applies spinning effect if requested app is handled by spinner controller.
  void MaybeApplySpinningEffect(const std::string& app_id,
                                gfx::ImageSkia* image);

  // Finishes spinning on an icon. If an icon is pinned it will be kept on the
  // shelf as a shortcut, otherwise it will be removed without storing the
  // delegate.
  void CloseSpinner(const std::string& app_id);

  // Closes all Crostini spinner shelf items.
  // This should be avoided when possible.
  void CloseCrostiniSpinners();

  Profile* OwnerProfile();

  // Hide all the spinners associated with the old user, and restore to the
  // shelf any spinners associated with the new active user. Called by
  // ChromeShelfController when the active user is changed.
  void ActiveUserChanged(const AccountId& account_id);

  // ash::ShelfModelObserver:
  void ShelfItemDelegateChanged(const ash::ShelfID& id,
                                ash::ShelfItemDelegate* old_delegate,
                                ash::ShelfItemDelegate* delegate) override;

 private:
  // Defines mapping of a shelf app id to a corresponding controller's data.
  // Shelf app id is optional mapping (for example, Play Store to ARC Host
  // Support).
  using AppControllerMap = std::map<std::string, ShelfSpinnerData>;
  // Defines a mapping from account id to (app id, ShelfSpinnerItemController)
  // for spinners that are not currently on the shelf. Taking ownership of these
  // delegates allows us to reuse them if we need to add the spinner back on to
  // the shelf.
  using HiddenAppControllerMap = std::multimap<
      AccountId,
      std::pair<std::string, std::unique_ptr<ShelfSpinnerItemController>>>;

  void UpdateApps();
  void UpdateShelfItemIcon(const std::string& app_id);
  void RegisterNextUpdate();
  // Removes the spinner with id |app_id| from |app_controller_map_| and returns
  // true if it was present, false otherwise.
  bool RemoveSpinnerFromControllerMap(const std::string& app_id);

  // Removes a spinner from the shelf and stores the delegate for later
  // restoration. Used when the user switches from one profile to another.
  void HideSpinner(const std::string& app_id);

  // Unowned pointers.
  raw_ptr<ChromeShelfController> owner_;

  AccountId current_account_id_;

  AppControllerMap app_controller_map_;

  HiddenAppControllerMap hidden_app_controller_map_;

  // Always keep this the last member of this class.
  base::WeakPtrFactory<ShelfSpinnerController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_SHELF_SPINNER_CONTROLLER_H_
