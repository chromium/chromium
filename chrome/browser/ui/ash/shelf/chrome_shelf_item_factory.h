// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_model.h"

class Profile;

// This class is responsible for converting app_ids from strings to a ShelfItem
// and ShelfItemDelegate that can be inserted into the ShelfModel.
class ChromeShelfItemFactory : public ash::ShelfModel::ShelfItemFactory {
 public:
  ChromeShelfItemFactory();
  virtual ~ChromeShelfItemFactory();

  // ShelfItemFactoryDelegate override:
  std::unique_ptr<ash::ShelfItem> CreateShelfItemForApp(
      const ash::ShelfID& shelf_id,
      ash::ShelfItemStatus status,
      ash::ShelfItemType shelf_item_type,
      const std::u16string& title) override;
  std::unique_ptr<ash::ShelfItemDelegate> CreateShelfItemDelegateForAppId(
      const std::string& app_id) override;

  void set_profile(Profile* profile) { profile_ = profile; }

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_
