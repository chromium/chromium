// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_

#include "ash/public/cpp/shelf_model.h"

class Profile;

// This class is responsible for converting app_ids from strings to a ShelfItem
// and ShelfItemDelegate that can be inserted into the ShelfModel.
class ChromeShelfItemFactory : public ash::ShelfModel::ShelfItemFactory {
 public:
  ChromeShelfItemFactory();
  virtual ~ChromeShelfItemFactory();

  // ShelfItemFactoryDelegate override:
  bool CreateShelfItemForAppId(
      const std::string& app_id,
      ash::ShelfItem* item,
      std::unique_ptr<ash::ShelfItemDelegate>* delegate) override;

 protected:
  // Virtual for testing. Returns the primary profile. Note that Lacros is
  // mutually exclusive with multi-signon, so the primary profile is the only
  // profile.
  virtual Profile* GetPrimaryProfile();
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_ITEM_FACTORY_H_
