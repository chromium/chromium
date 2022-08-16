// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/time/time.h"
#include "components/app_restore/restore_data.h"
#include "url/gurl.h"

namespace ash {
class DeskTemplate;
enum class DeskTemplateSource;
enum class DeskTemplateType;
}  // namespace ash

namespace desks_storage {

// Helper class for building a saved desk for test.
class SavedDeskBuilder {
 public:
  SavedDeskBuilder();
  SavedDeskBuilder(const SavedDeskBuilder&) = delete;
  SavedDeskBuilder& operator=(const SavedDeskBuilder&) = delete;
  ~SavedDeskBuilder();

  // Builds a saved desk. This should only be called once per builder
  // instance.
  std::unique_ptr<ash::DeskTemplate> Build();

  // Sets saved desk UUID. If not set, the built desk will have a random UUID.
  SavedDeskBuilder& SetUuid(const std::string& uuid);

  // Sets saved desk name. If not set, the built desk will have a fixed name.
  SavedDeskBuilder& SetName(const std::string& name);

  // Sets saved desk type. If not set, the built desk will default to
  // DeskTemplate.
  SavedDeskBuilder& SetType(ash::DeskTemplateType desk_type);

  // Sets saved desk source. If not set, the built desk will default to kUser.
  SavedDeskBuilder& SetSource(ash::DeskTemplateSource desk_source);

  // Sets saved desk creation timestamp. If not set, the built desk will have
  // its creation timestamp set at the creation time of the SavedDeskBuilder.
  SavedDeskBuilder& SetCreatedTime(base::Time& created_time);

  // Adds a Ash Chrome Browser App window.
  SavedDeskBuilder& AddAshBrowserAppWindow(int window_id,
                                           std::vector<GURL> urls);

  // Adds a Lacros Chrome Browser App window.
  SavedDeskBuilder& AddLacrosBrowserAppWindow(int window_id,
                                              std::vector<GURL> urls);

  // Adds a PWA window hosted in Ash Chrome.
  SavedDeskBuilder& AddAshPwaAppWindow(int window_id, const std::string url);

  // Adds a PWA window hosted in Lacros Chrome.
  SavedDeskBuilder& AddLacrosPwaAppWindow(int window_id, const std::string url);

  // Adds a Chrome app window.
  SavedDeskBuilder& AddChromeAppWindow(int window_id, const std::string app_id);

  // Adds a generic app window.
  SavedDeskBuilder& AddGenericAppWindow(int window_id,
                                        const std::string app_id);

 private:
  base::GUID desk_uuid_;
  std::string desk_name_;
  ash::DeskTemplateSource desk_source_;
  ash::DeskTemplateType desk_type_;
  base::Time created_time_;
  std::unique_ptr<app_restore::RestoreData> restore_data_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_
