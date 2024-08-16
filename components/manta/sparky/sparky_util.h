// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_
#define COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"

namespace manta {

enum class Role {
  kUser = 0,
  kAssistant = 1,
  kMaxValue = kAssistant,
};

// TODO(b/351099209): Add kKeyPress to actions.

enum class ActionType {
  kSetting = 0,
  kLaunchApp = 1,
  kLaunchFile = 2,
  kTextEntry = 3,
  kClick = 4,
  kAllDone = 5,
  kMaxValue = kAllDone,
};

struct COMPONENT_EXPORT(MANTA) FileAction {
  explicit FileAction(std::string launch_file_path);

  ~FileAction();
  FileAction(const FileAction&);
  FileAction& operator=(const FileAction&);

  std::string launch_file_path;
};

struct COMPONENT_EXPORT(MANTA) ClickAction {
  ClickAction(int x_pos, int y_pos);

  ~ClickAction();
  ClickAction(const ClickAction&);
  ClickAction& operator=(const ClickAction&);

  int x_pos;
  int y_pos;
};

struct COMPONENT_EXPORT(MANTA) Action {
  explicit Action(SettingsData updated_setting);
  explicit Action(ClickAction click);
  Action(FileAction file_action, ActionType type);
  explicit Action(bool all_done);
  explicit Action(ActionType type);

  ~Action();
  Action(const Action&);
  Action& operator=(const Action&);

  std::string launched_app;
  std::optional<SettingsData> updated_setting;
  std::optional<ClickAction> click;
  std::string text_entry;
  std::optional<FileAction> file_action;
  ActionType type;
  bool all_done;
};

struct COMPONENT_EXPORT(MANTA) DialogTurn {
  DialogTurn(const std::string& message,
             Role role,
             std::vector<Action> actions);
  DialogTurn(const std::string& message, Role role);

  ~DialogTurn();

  DialogTurn(const DialogTurn&);
  DialogTurn& operator=(const DialogTurn&);

  void AppendAction(Action action);

  std::string message;
  Role role;
  std::vector<Action> actions;
};

proto::Role COMPONENT_EXPORT(MANTA) GetRole(Role role);

void COMPONENT_EXPORT(MANTA)
    AddSettingProto(const SettingsData& setting,
                    ::manta::proto::Setting* setting_proto,
                    proto::SettingType setting_type);

void COMPONENT_EXPORT(MANTA)
    AddSettingsProto(const SparkyDelegate::SettingsDataList& settings_list,
                     ::manta::proto::SettingsData* settings_data);

std::vector<Diagnostics> COMPONENT_EXPORT(MANTA)
    ObtainDiagnosticsVectorFromProto(
        const ::manta::proto::DiagnosticsRequest& diagnostics_request);

void COMPONENT_EXPORT(MANTA)
    AddDiagnosticsProto(std::optional<DiagnosticsData> diagnostics_data,
                        proto::DiagnosticsData* diagnostics_proto);

void COMPONENT_EXPORT(MANTA) AddAppsData(base::span<const AppsData> apps_data,
                                         proto::AppsData* apps_proto);

std::unique_ptr<SettingsData> COMPONENT_EXPORT(MANTA)
    ObtainSettingFromProto(proto::Setting setting_proto);

DialogTurn COMPONENT_EXPORT(MANTA)
    ConvertDialogToStruct(proto::Turn* turn_proto);

void COMPONENT_EXPORT(MANTA)
    AddDialogToSparkyContext(const std::vector<DialogTurn>& dialog,
                             proto::SparkyContextData* sparky_context_proto);

void COMPONENT_EXPORT(MANTA) AddFilesData(base::span<const FileData> files_data,
                                          proto::FilesData* files_proto);

std::set<std::string> COMPONENT_EXPORT(MANTA)
    GetSelectedFilePaths(const proto::FileRequest& file_request);

std::optional<FileData> COMPONENT_EXPORT(MANTA)
    GetFileFromProto(const proto::File& files_proto);

std::vector<FileData> COMPONENT_EXPORT(MANTA)
    GetFileDataFromProto(const proto::FilesData& files_proto);

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_UTIL_H_
