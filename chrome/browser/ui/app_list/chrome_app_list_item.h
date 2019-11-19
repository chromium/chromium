// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "ui/gfx/image/image_skia.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class Profile;

namespace extensions {
class AppSorting;
}  // namespace extensions

namespace ui {
class SimpleMenuModel;
}  // namespace ui

// Base class of all chrome app list items.
class ChromeAppListItem {
 public:
  class TestApi {
   public:
    explicit TestApi(ChromeAppListItem* item);
    ~TestApi() = default;

    void SetFolderId(const std::string& folder_id);
    void SetPosition(const syncer::StringOrdinal& position);

   private:
    ChromeAppListItem* const item_;
  };

  ChromeAppListItem(Profile* profile,
                    const std::string& app_id,
                    AppListModelUpdater* model_updater);
  virtual ~ChromeAppListItem();

  // AppListControllerDelegate is not properly implemented in tests. Use mock
  // |controller| for unit_tests.
  static void OverrideAppListControllerDelegateForTesting(
      AppListControllerDelegate* controller);

  static gfx::ImageSkia CreateDisabledIcon(const gfx::ImageSkia& icon);

  const std::string& id() const { return metadata_->id; }
  const std::string& folder_id() const { return metadata_->folder_id; }
  const syncer::StringOrdinal& position() const { return metadata_->position; }
  const std::string& name() const { return metadata_->name; }
  bool is_folder() const { return metadata_->is_folder; }
  bool is_persistent() const { return metadata_->is_persistent; }
  const gfx::ImageSkia& icon() const { return metadata_->icon; }
  bool is_page_break() const { return metadata_->is_page_break; }

  void SetIsInstalling(bool is_installing);
  void SetPercentDownloaded(int32_t percent_downloaded);

  void SetMetadata(std::unique_ptr<ash::AppListItemMetadata> metadata);
  std::unique_ptr<ash::AppListItemMetadata> CloneMetadata() const;

  // The following methods set Chrome side data here, and call model updater
  // interfaces that talk to ash directly.
  void SetIcon(const gfx::ImageSkia& icon);
  void SetName(const std::string& name);
  void SetNameAndShortName(const std::string& name,
                           const std::string& short_name);
  void SetFolderId(const std::string& folder_id);
  void SetPosition(const syncer::StringOrdinal& position);
  void SetIsPageBreak(bool is_page_break);
  void SetIsPersistent(bool is_persistent);

  // The following methods won't make changes to Ash and it should be called
  // by this item itself or the model updater.
  void SetChromeFolderId(const std::string& folder_id);
  void SetChromeIsFolder(bool is_folder);
  void SetChromeName(const std::string& name);
  void SetChromePosition(const syncer::StringOrdinal& position);

  // Call |Activate()| and dismiss launcher if necessary.
  void PerformActivate(int event_flags);

  // Activates (opens) the item. Does nothing by default.
  virtual void Activate(int event_flags);

  // Returns a static const char* identifier for the subclass (defaults to "").
  // Pointers can be compared for quick type checking.
  virtual const char* GetItemType() const;

  // Returns the context menu model in |callback| for this item. NULL if there
  // is currently no menu for the item (e.g. during install). Note |callback|
  // takes the ownership of the returned menu model.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(GetMenuModelCallback callback);

  // Returns true iff this item was badged because it's an extension app that
  // has its Android analog installed.
  virtual bool IsBadged() const;

  bool CompareForTest(const ChromeAppListItem* other) const;

  std::string ToDebugString() const;

  // Set the default position if it exists. Otherwise set the first available
  // position in the app list if |model_updater| is not null.
  void SetDefaultPositionIfApplicable(AppListModelUpdater* model_updater);

 protected:
  ChromeAppListItem(Profile* profile, const std::string& app_id);

  Profile* profile() const { return profile_; }

  extensions::AppSorting* GetAppSorting();

  AppListControllerDelegate* GetController();

  AppListModelUpdater* model_updater() { return model_updater_; }
  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }

  // Updates item position and name from |sync_item|. |sync_item| must be valid.
  void UpdateFromSync(
      const app_list::AppListSyncableService::SyncItem* sync_item);

  // Get the context menu of a certain app. This could be different for
  // different kinds of items.
  virtual app_list::AppContextMenu* GetAppContextMenu();

  void MaybeDismissAppList();

 private:
  std::unique_ptr<ash::AppListItemMetadata> metadata_;
  Profile* profile_;
  AppListModelUpdater* model_updater_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppListItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
