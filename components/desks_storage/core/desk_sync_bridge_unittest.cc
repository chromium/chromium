// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <stddef.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_storage_metrics_util.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/saved_desk_builder.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using BrowserAppTab =
    sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_BrowserAppTab;
using ArcApp = sync_pb::WorkspaceDeskSpecifics_ArcApp;
using ArcSize = sync_pb::WorkspaceDeskSpecifics_ArcApp_WindowSize;
using BrowserAppWindow = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow;
using ChromeApp = sync_pb::WorkspaceDeskSpecifics_ChromeApp;
using Desk = sync_pb::WorkspaceDeskSpecifics_Desk;
using ProgressiveWebApp = sync_pb::WorkspaceDeskSpecifics_ProgressiveWebApp;
using SyncDeskType = sync_pb::WorkspaceDeskSpecifics_DeskType;
using DeskType = ash::DeskTemplateType;
using WindowBound = sync_pb::WorkspaceDeskSpecifics_WindowBound;
using WindowState = sync_pb::WorkspaceDeskSpecifics_WindowState;
using WorkspaceDeskSpecifics_App = sync_pb::WorkspaceDeskSpecifics_App;
using SyncTabGroup = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_TabGroup;
using SyncTabGroupColor = sync_pb::WorkspaceDeskSpecifics_TabGroupColor;
using TestUuidId = base::StrongAlias<class TestUuidIdTag, int>;

namespace {

using ash::DeskTemplate;
using ash::DeskTemplateSource;
using ash::DeskTemplateType;
using sync_pb::DataTypeState;
using sync_pb::WorkspaceDeskSpecifics;
using syncer::DataTypeStore;
using syncer::DataTypeStoreTestUtil;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::HasEncryptionKeyName;
using syncer::InMemoryMetadataChangeList;
using syncer::MetadataBatchContains;
using syncer::MetadataChangeList;
using syncer::MockDataTypeLocalChangeProcessor;
using syncer::ModelError;
using testing::_;
using testing::Return;
using testing::SizeIs;
using testing::StrEq;

constexpr char kUuidFormat[] = "9e186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kNameFormat[] = "template %d";
constexpr char kTestUrlFormat[] = "https://www.testdomain%d.com/";
constexpr char kTestAppNameFormat[] = "_some_prefix_%s";
constexpr int kDefaultTemplateIndex = 1;
constexpr int kBrowserWindowId = 1555;
constexpr int kPwaWindowId = 1666;
constexpr int kChromeAppWindowId = 1777;

// Example app index as set in `ExampleWorkspaceDeskSpecifics`.
constexpr int kExampleDeskChromeAppIndex = 2;
constexpr int kExampleDeskProgressiveWebAppIndex = 3;

constexpr auto kWindowOpenDispositionValues = base::MakeFixedFlatSet<
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition>(
    {sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_UNKNOWN,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_CURRENT_TAB,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SINGLETON_TAB,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_FOREGROUND_TAB,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_BACKGROUND_TAB,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_POPUP,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SAVE_TO_DISK,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_OFF_THE_RECORD,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_IGNORE_ACTION,
     sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SWITCH_TO_TAB,
     sync_pb::
         WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_PICTURE_IN_PICTURE});

constexpr auto kLaunchContainerValues = base::MakeFixedFlatSet<
    sync_pb::WorkspaceDeskSpecifics_LaunchContainer>({
    sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW,
    sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED,
    sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_TAB,
    sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_NONE,
});

constexpr auto kTabGroupColors = base::MakeFixedFlatSet<SyncTabGroupColor>(
    {SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREY,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_BLUE,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_RED,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_YELLOW,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREEN,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PINK,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PURPLE,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_CYAN,
     SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_ORANGE});

base::Uuid MakeTestUuid(TestUuidId uuid_id) {
  return base::Uuid::ParseCaseInsensitive(
      base::StringPrintf(kUuidFormat, uuid_id.value()));
}

base::Uuid MakeAdminTestUuid(TestUuidId uuid_id) {
  return base::Uuid::ParseCaseInsensitive(base::StringPrintf(
      "59dbe2b8-671f-4fd0-92ec-11111111100%d", uuid_id.value()));
}

std::string GetPolicyWithTwoTemplates() {
  return "[{\"version\":1,\"uuid\":\"" + base::StringPrintf(kUuidFormat, 8) +
         "\",\"name\":\""
         "Example Template"
         "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
         "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{"
         "\"window_"
         "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
         "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
         "\"url\":\"https://"
         "example.com\",\"title\":\"Example\"},{\"url\":\"https://"
         "example.com/"
         "2\",\"title\":\"Example2\"}],\"tab_groups\":[{\"range_"
         "start\":1,\"range_end\":2,\"title\":\"sample_tab_"
         "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_"
         "index\":"
         "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
         "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}},"
         "{\"version\":1,\"uuid\":\"" +
         base::StringPrintf(kUuidFormat, 9) +
         "\",\"name\":\""
         "Example Template 2"
         "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
         "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{"
         "\"window_"
         "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
         "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
         "\"url\":\"https://google.com\",\"title\":\"Example "
         "2\"},{\"url\":\"https://"
         "gmail.com.com/"
         "2\",\"title\":\"Example2\"}],\"active_tab_index\":1,\"first_non_"
         "pinned_"
         "tab_index\":1,\"window_id\":0,"
         "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}}"
         "]";
}

void FillDefaultBrowserAppWindow(WorkspaceDeskSpecifics_App* app,
                                 BrowserAppWindow* app_window,
                                 int number_of_tabs) {
  for (int i = 0; i < number_of_tabs; ++i) {
    BrowserAppTab* tab = app_window->add_tabs();
    tab->set_url(base::StringPrintf(kTestUrlFormat, i));
  }

  app_window->set_active_tab_index(number_of_tabs - 1);
  app_window->set_first_non_pinned_tab_index(number_of_tabs - 1);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(110);
  window_bound->set_top(120);
  window_bound->set_width(1330);
  window_bound->set_height(1440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED);
  app->set_display_id(99887766l);
  app->set_z_index(133);
  app->set_window_id(1555);
  app->set_snap_percentage(75);
}

void FillExampleBrowserAppWindow(WorkspaceDeskSpecifics_App* app,
                                 int number_of_tabs = 2) {
  BrowserAppWindow* app_window =
      app->mutable_app()->mutable_browser_app_window();

  FillDefaultBrowserAppWindow(app, app_window, number_of_tabs);

  SyncTabGroup* tab_group = app_window->add_tab_groups();
  tab_group->set_first_index(1);
  tab_group->set_last_index(2);
  tab_group->set_title("test_tab_group");
  tab_group->set_color(
      SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREEN);
}

void FillExampleBrowserAppWindow(WorkspaceDeskSpecifics_App* app,
                                 int tab_group_first_index,
                                 int tab_group_last_index,
                                 const std::string& tab_group_title,
                                 bool tab_group_is_collapsed,
                                 SyncTabGroupColor tab_group_color) {
  BrowserAppWindow* app_window =
      app->mutable_app()->mutable_browser_app_window();

  FillDefaultBrowserAppWindow(app, app_window, tab_group_last_index);

  SyncTabGroup* tab_group = app_window->add_tab_groups();
  tab_group->set_first_index(tab_group_first_index);
  tab_group->set_last_index(tab_group_last_index);
  tab_group->set_title(tab_group_title);
  if (tab_group_is_collapsed)
    tab_group->set_is_collapsed(tab_group_is_collapsed);
  tab_group->set_color(tab_group_color);
}

void FillExampleProgressiveWebAppWindow(WorkspaceDeskSpecifics_App* app) {
  ChromeApp* app_window = app->mutable_app()->mutable_chrome_app();
  app_window->set_app_id(desk_test_util::kTestPwaAppId);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(210);
  window_bound->set_top(220);
  window_bound->set_width(2330);
  window_bound->set_height(2440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED);
  app->set_pre_minimized_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN);
  app->set_container(
      sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW);
  app->set_disposition(
      sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW);
  app->set_app_name(
      base::StringPrintf(kTestAppNameFormat, desk_test_util::kTestPwaAppId));
  app->set_display_id(99887766l);
  app->set_z_index(233);
  app->set_window_id(2555);
}

void FillExampleSystemWebAppWindow(WorkspaceDeskSpecifics_App* app) {
  ChromeApp* app_window = app->mutable_app()->mutable_chrome_app();
  app_window->set_app_id(desk_test_util::kTestSwaAppId);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(220);
  window_bound->set_top(230);
  window_bound->set_width(2340);
  window_bound->set_height(2450);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED);
  app->set_pre_minimized_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN);
  app->set_container(
      sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW);
  app->set_disposition(
      sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW);
  app->set_app_name(
      base::StringPrintf(kTestAppNameFormat, desk_test_util::kTestSwaAppId));
  app->set_display_id(99887766l);
  app->set_z_index(234);
  app->set_window_id(2556);
}

void FillExampleChromeAppWindow(WorkspaceDeskSpecifics_App* app) {
  ChromeApp* app_window = app->mutable_app()->mutable_chrome_app();
  app_window->set_app_id(desk_test_util::kTestChromeAppId);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(210);
  window_bound->set_top(220);
  window_bound->set_width(2330);
  window_bound->set_height(2440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED);
  app->set_container(
      sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED);
  app->set_disposition(
      sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW);
  app->set_app_name(
      base::StringPrintf(kTestAppNameFormat, desk_test_util::kTestChromeAppId));
  app->set_display_id(99887766l);
  app->set_z_index(233);
  app->set_window_id(2555);
}

void FillExampleArcAppWindow(WorkspaceDeskSpecifics_App* app) {
  ArcApp* app_window = app->mutable_app()->mutable_arc_app();
  app_window->set_app_id(desk_test_util::kTestArcAppId);

  ArcSize* minimum_size = app_window->mutable_minimum_size();
  minimum_size->set_width(1);
  minimum_size->set_height(1);

  ArcSize* maximum_size = app_window->mutable_maximum_size();
  maximum_size->set_width(256);
  maximum_size->set_height(256);

  WindowBound* bounds_in_root = app_window->mutable_bounds_in_root();
  bounds_in_root->set_width(1024);
  bounds_in_root->set_height(1024);
  bounds_in_root->set_left(0);
  bounds_in_root->set_top(0);

  WindowBound* window_bound = app->mutable_window_bound();
  window_bound->set_left(210);
  window_bound->set_top(220);
  window_bound->set_width(2330);
  window_bound->set_height(2440);
  app->set_window_state(
      WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED);
  app->set_app_name(
      base::StringPrintf(kTestAppNameFormat, desk_test_util::kTestArcAppId));
  app->set_display_id(99887766l);
  app->set_z_index(233);
  app->set_window_id(2555);
  app->set_title("test_arc_app_title");
}

WorkspaceDeskSpecifics ExampleWorkspaceDeskSpecificsWithoutDeskType(
    const std::string uuid,
    const std::string template_name,
    base::Time created_time = base::Time::Now(),
    int number_of_tabs = 2) {
  WorkspaceDeskSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_name(template_name);
  specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_updated_time_windows_epoch_micros(
      (created_time + base::Minutes(5))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  Desk* desk = specifics.mutable_desk();
  FillExampleBrowserAppWindow(desk->add_apps(), number_of_tabs);
  FillExampleArcAppWindow(desk->add_apps());
  FillExampleChromeAppWindow(desk->add_apps());
  FillExampleProgressiveWebAppWindow(desk->add_apps());
  FillExampleSystemWebAppWindow(desk->add_apps());
  specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  return specifics;
}

WorkspaceDeskSpecifics ExampleWorkspaceDeskSpecifics(
    const std::string uuid,
    const std::string template_name,
    base::Time created_time = base::Time::Now(),
    int number_of_tabs = 2,
    SyncDeskType desk_type =
        SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE) {
  WorkspaceDeskSpecifics specifics =
      ExampleWorkspaceDeskSpecificsWithoutDeskType(
          uuid, template_name, created_time, number_of_tabs);
  specifics.set_desk_type(desk_type);
  specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  return specifics;
}

WorkspaceDeskSpecifics ExampleWorkspaceDeskSpecifics(
    const std::string& uuid,
    const std::string& template_name,
    int tab_group_first_index,
    int tab_group_last_index,
    const std::string& tab_group_title,
    bool tab_group_is_collapsed,
    SyncTabGroupColor tab_group_color) {
  base::Time created_time = base::Time::Now();

  WorkspaceDeskSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_name(template_name);
  specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_updated_time_windows_epoch_micros(
      (created_time + base::Minutes(5))
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  specifics.set_desk_type(
      SyncDeskType::WorkspaceDeskSpecifics_DeskType_SAVE_AND_RECALL);
  Desk* desk = specifics.mutable_desk();
  FillExampleBrowserAppWindow(desk->add_apps(), tab_group_first_index,
                              tab_group_last_index, tab_group_title,
                              tab_group_is_collapsed, tab_group_color);
  FillExampleArcAppWindow(desk->add_apps());
  FillExampleChromeAppWindow(desk->add_apps());
  FillExampleProgressiveWebAppWindow(desk->add_apps());
  FillExampleSystemWebAppWindow(desk->add_apps());
  specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  return specifics;
}

WorkspaceDeskSpecifics CreateWorkspaceDeskSpecifics(
    int templateIndex,
    base::Time created_time = base::Time::Now()) {
  return ExampleWorkspaceDeskSpecifics(
      base::StringPrintf(kUuidFormat, templateIndex),
      base::StringPrintf(kNameFormat, templateIndex), created_time);
}

WorkspaceDeskSpecifics CreateUnknownDeskType() {
  return ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(),
      base::StringPrintf(kNameFormat, 1), base::Time::Now(),
      /*number_of_tabs=*/2,
      SyncDeskType::WorkspaceDeskSpecifics_DeskType_UNKNOWN_TYPE);
}

WorkspaceDeskSpecifics CreateWorkspaceDeskWithoutDeskType() {
  return ExampleWorkspaceDeskSpecificsWithoutDeskType(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(),
      base::StringPrintf(kNameFormat, 1), base::Time::Now(),
      /*number_of_tabs=*/2);
}

WorkspaceDeskSpecifics CreateFloatingWorkspaceTemplateExpectedValue(
    std::string cache_guid) {
  WorkspaceDeskSpecifics expected_desk_specifics =
      ExampleWorkspaceDeskSpecifics(
          MakeTestUuid(TestUuidId(1)).AsLowercaseString(),
          base::StringPrintf(kNameFormat, 1), base::Time::Now(),
          /*number_of_tabs=*/2,
          SyncDeskType::WorkspaceDeskSpecifics_DeskType_FLOATING_WORKSPACE);
  expected_desk_specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  expected_desk_specifics.set_client_cache_guid(cache_guid);
  return expected_desk_specifics;
}

WorkspaceDeskSpecifics CreateBrowserTemplateExpectedValue(
    int template_index,
    const base::Time& created_time) {
  WorkspaceDeskSpecifics expected_desk_specifics;
  expected_desk_specifics.set_uuid(
      base::StringPrintf(kUuidFormat, template_index));
  expected_desk_specifics.set_name(
      base::StringPrintf(kNameFormat, template_index));
  expected_desk_specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_desk_specifics.set_desk_type(
      SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE);
  expected_desk_specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  Desk* expected_desk = expected_desk_specifics.mutable_desk();
  WorkspaceDeskSpecifics_App* app = expected_desk->add_apps();
  app->set_window_id(kBrowserWindowId);
  BrowserAppWindow* browser_window =
      app->mutable_app()->mutable_browser_app_window();

  BrowserAppTab* first_tab = browser_window->add_tabs();
  first_tab->set_url(GURL(base::StringPrintf(kTestUrlFormat, 1)).spec());
  BrowserAppTab* second_tab = browser_window->add_tabs();
  second_tab->set_url(GURL(base::StringPrintf(kTestUrlFormat, 2)).spec());

  return expected_desk_specifics;
}

WorkspaceDeskSpecifics CreatePwaTemplateExpectedValue(
    int template_index,
    const base::Time& created_time) {
  WorkspaceDeskSpecifics expected_desk_specifics;
  expected_desk_specifics.set_uuid(
      base::StringPrintf(kUuidFormat, template_index));
  expected_desk_specifics.set_name(
      base::StringPrintf(kNameFormat, template_index));
  expected_desk_specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_desk_specifics.set_desk_type(
      SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE);
  expected_desk_specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  Desk* expected_desk = expected_desk_specifics.mutable_desk();
  WorkspaceDeskSpecifics_App* app = expected_desk->add_apps();
  app->set_window_id(kPwaWindowId);
  BrowserAppWindow* browser_window =
      app->mutable_app()->mutable_browser_app_window();

  BrowserAppTab* first_tab = browser_window->add_tabs();
  first_tab->set_url(GURL(base::StringPrintf(kTestUrlFormat, 1)).spec());

  browser_window->set_show_as_app(true);

  return expected_desk_specifics;
}

WorkspaceDeskSpecifics CreateChromeAppTemplateExpectedValue(
    int template_index,
    const base::Time& created_time,
    int window_id,
    const std::string& app_id) {
  WorkspaceDeskSpecifics expected_desk_specifics;
  expected_desk_specifics.set_uuid(
      base::StringPrintf(kUuidFormat, template_index));
  expected_desk_specifics.set_name(
      base::StringPrintf(kNameFormat, template_index));
  expected_desk_specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_desk_specifics.set_desk_type(
      SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE);
  expected_desk_specifics.set_device_form_factor(
      sync_pb::SyncEnums::DeviceFormFactor::
          SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  Desk* expected_desk = expected_desk_specifics.mutable_desk();
  WorkspaceDeskSpecifics_App* app = expected_desk->add_apps();
  app->set_window_id(window_id);

  ChromeApp* app_window = app->mutable_app()->mutable_chrome_app();
  app_window->set_app_id(app_id);

  return expected_desk_specifics;
}

DataTypeState StateWithEncryption(const std::string& encryption_key_name) {
  DataTypeState state;
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

class MockDeskModelObserver : public DeskModelObserver {
 public:
  MOCK_METHOD0(DeskModelLoaded, void());
  MOCK_METHOD1(
      EntriesAddedOrUpdatedRemotely,
      void(
          const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&));
  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<base::Uuid>&));
};

MATCHER_P(UuidIs, e, "") {
  return testing::ExplainMatchResult(e, arg->uuid(), result_listener);
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "Expected:\n"
                     << expected.SerializeAsString() << "\nActual\n"
                     << arg.SerializeAsString() << "\n";
    return false;
  }
  return true;
}

class DeskSyncBridgeTest : public testing::Test {
 public:
  DeskSyncBridgeTest(const DeskSyncBridgeTest&) = delete;
  DeskSyncBridgeTest& operator=(const DeskSyncBridgeTest&) = delete;

 protected:
  DeskSyncBridgeTest()
      : store_(DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")) {}

  void CreateBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        account_id_);
    bridge_->AddObserver(&mock_observer_);
  }

  void FinishInitialization() { base::RunLoop().RunUntilIdle(); }

  void InitializeBridge() {
    CreateBridge();
    FinishInitialization();
  }

  void DisableBridgeSync() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  void ShutdownBridge() {
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(&mock_observer_);
  }

  void RestartBridge() {
    ShutdownBridge();
    InitializeBridge();
  }

  void WriteToStoreWithMetadata(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list,
      DataTypeState state) {
    std::unique_ptr<DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
    }
    batch->GetMetadataChangeList()->UpdateDataTypeState(state);
    CommitToStoreAndWait(std::move(batch));
  }

  void CommitToStoreAndWait(std::unique_ptr<DataTypeStore::WriteBatch> batch) {
    base::RunLoop loop;
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop, const std::optional<ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  EntityData MakeEntityData(
      const WorkspaceDeskSpecifics& workspace_desk_specifics) {
    EntityData entity_data;

    *entity_data.specifics.mutable_workspace_desk() = workspace_desk_specifics;

    entity_data.name = workspace_desk_specifics.name();
    return entity_data;
  }

  EntityData MakeEntityData(const DeskTemplate& desk_template) {
    return MakeEntityData(
        desk_template_conversion::ToSyncProto(&desk_template, app_cache()));
  }

  // Helper method to reduce duplicated code between tests. Wraps the given
  // specifics objects in an EntityData and EntityChange of type ACTION_ADD,
  // and returns an EntityChangeList containing them all. Order is maintained.
  EntityChangeList EntityAddList(
      const std::vector<WorkspaceDeskSpecifics>& specifics_list) {
    EntityChangeList changes;
    for (const auto& specifics : specifics_list) {
      changes.push_back(
          EntityChange::CreateAdd(specifics.uuid(), MakeEntityData(specifics)));
    }
    return changes;
  }

  base::Time AdvanceAndGetTime(base::TimeDelta delta = base::Milliseconds(10)) {
    clock_.Advance(delta);
    return clock_.Now();
  }

  void AddTwoTemplates() {
    auto desk_template1 =
        desk_template_conversion::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1",
            AdvanceAndGetTime()));
    auto desk_template2 =
        desk_template_conversion::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            MakeTestUuid(TestUuidId(2)).AsLowercaseString(), "template 2",
            AdvanceAndGetTime()));

    base::RunLoop loop1;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template1),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template2),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void AddTwoTemplatesWithDuplicatedNames() {
    // These two templates will have new UUIDs but with names that collides
    // with "template 1"
    auto desk_template1 =
        desk_template_conversion::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            MakeTestUuid(TestUuidId(8)).AsLowercaseString(), "template 1",
            AdvanceAndGetTime()));
    auto desk_template2 =
        desk_template_conversion::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            MakeTestUuid(TestUuidId(9)).AsLowercaseString(), "template 1",
            AdvanceAndGetTime()));

    base::RunLoop loop1;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template1),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop1.Quit();
            }));
    loop1.Run();

    base::RunLoop loop2;
    bridge()->AddOrUpdateEntry(
        std::move(desk_template2),
        base::BindLambdaForTesting(
            [&](DeskModel::AddOrUpdateEntryStatus status,
                std::unique_ptr<ash::DeskTemplate> new_entry) {
              EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
              loop2.Quit();
            }));
    loop2.Run();
  }

  void SetOneAdminTemplate() {
    auto admin_template1 =
        desk_template_conversion::FromSyncProto(ExampleWorkspaceDeskSpecifics(
            MakeAdminTestUuid(TestUuidId(1)).AsLowercaseString(),
            "admin template 1", AdvanceAndGetTime()));

    std::string policy_json;
    base::Value::List template_list;
    template_list.Append(
        desk_template_conversion::SerializeDeskTemplateAsBaseValue(
            admin_template1.get(), cache_.get()));
    bool conversion_success =
        base::JSONWriter::Write(template_list, &policy_json);
    EXPECT_TRUE(conversion_success);

    bridge()->SetPolicyDeskTemplates(policy_json);
  }

  // testing::test.
  void SetUp() override {
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
  }

  MockDataTypeLocalChangeProcessor* processor() { return &mock_processor_; }

  DeskSyncBridge* bridge() { return bridge_.get(); }

  MockDeskModelObserver* mock_observer() { return &mock_observer_; }

  base::SimpleTestClock* clock() { return &clock_; }

  apps::AppRegistryCache* app_cache() { return cache_.get(); }

 private:
  base::SimpleTestClock clock_;

  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<DataTypeStore> store_;

  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;

  testing::NiceMock<MockDeskModelObserver> mock_observer_;

  std::unique_ptr<DeskSyncBridge> bridge_;

  std::unique_ptr<apps::AppRegistryCache> cache_;

  AccountId account_id_;
};

TEST_F(DeskSyncBridgeTest, DeskTemplateConversionShouldBeLossless) {
  CreateBridge();

  WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1");

  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(desk_proto);
  WorkspaceDeskSpecifics converted_desk_proto =
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

  std::unique_ptr<DeskTemplate> converted_desk_template =
      desk_template_conversion::FromSyncProto(converted_desk_proto);

  EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
}

TEST_F(DeskSyncBridgeTest, DeskTemplateJsonConversionShouldBeLossless) {
  CreateBridge();

  WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1");

  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(desk_proto);

  base::Value template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.get(), app_cache());

  auto converted_desk_template =
      desk_template_conversion::ParseDeskTemplateFromBaseValue(
          template_value, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(converted_desk_template.has_value());
  EXPECT_EQ(
      desk_template->desk_restore_data()->ConvertToValue(),
      converted_desk_template.value()->desk_restore_data()->ConvertToValue());

  WorkspaceDeskSpecifics converted_desk_proto =
      desk_template_conversion::ToSyncProto(
          converted_desk_template.value().get(), app_cache());

  EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
}

TEST_F(DeskSyncBridgeTest, AppNameConversionShouldBeLossless) {
  constexpr int kExampleDeskBrowserAppIndex = 0;
  constexpr int kExampleDeskArcAppIndex = 1;
  constexpr int kExampleDeskSystemWebAppIndex = 4;

  CreateBridge();

  WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1");

  desk_proto.mutable_desk()
      ->mutable_apps(kExampleDeskBrowserAppIndex)
      ->set_app_name("app name 1");
  desk_proto.mutable_desk()
      ->mutable_apps(kExampleDeskArcAppIndex)
      ->set_app_name("app name 2");
  desk_proto.mutable_desk()
      ->mutable_apps(kExampleDeskChromeAppIndex)
      ->set_app_name("app name 3");
  desk_proto.mutable_desk()
      ->mutable_apps(kExampleDeskProgressiveWebAppIndex)
      ->set_app_name("app name 4");
  desk_proto.mutable_desk()
      ->mutable_apps(kExampleDeskSystemWebAppIndex)
      ->set_app_name("app name 5");

  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(desk_proto);

  WorkspaceDeskSpecifics converted_desk_proto =
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

  EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
}

TEST_F(DeskSyncBridgeTest, WindowOpenDispositionConversionShouldBeLossless) {
  CreateBridge();

  for (const auto& test_value : kWindowOpenDispositionValues) {
    WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
        MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1");

    desk_proto.mutable_desk()
        ->mutable_apps(kExampleDeskChromeAppIndex)
        ->set_disposition(test_value);
    desk_proto.mutable_desk()
        ->mutable_apps(kExampleDeskProgressiveWebAppIndex)
        ->set_disposition(test_value);

    std::unique_ptr<DeskTemplate> desk_template =
        desk_template_conversion::FromSyncProto(desk_proto);

    WorkspaceDeskSpecifics converted_desk_proto =
        desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

    EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
  }
}

TEST_F(DeskSyncBridgeTest, LaunchContainerConversionShouldBeLossless) {
  CreateBridge();

  for (const auto& test_value : kLaunchContainerValues) {
    WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
        MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1");

    desk_proto.mutable_desk()
        ->mutable_apps(kExampleDeskChromeAppIndex)
        ->set_container(test_value);
    desk_proto.mutable_desk()
        ->mutable_apps(kExampleDeskProgressiveWebAppIndex)
        ->set_container(test_value);

    std::unique_ptr<DeskTemplate> desk_template =
        desk_template_conversion::FromSyncProto(desk_proto);

    WorkspaceDeskSpecifics converted_desk_proto =
        desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

    EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
  }
}

TEST_F(DeskSyncBridgeTest, TabGroupInfoConversionShouldBeLossless) {
  CreateBridge();

  // Iterate over colors, changing the values contained within the
  // tab group for each iteration.
  size_t curr_start = 0;
  for (const auto& test_color_value : kTabGroupColors) {
    // We test with the color GREEN by default.
    if (test_color_value ==
        SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREEN) {
      continue;
    }

    WorkspaceDeskSpecifics desk_proto = ExampleWorkspaceDeskSpecifics(
        MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1",
        /*Set a different range for each iteration.*/
        curr_start, curr_start + 1,
        /*Change name for each iteration.*/
        base::StringPrintf("test_tab_group_%zu", curr_start),
        /*Modulate between true and false for is_collapsed value on each
           iteration.*/
        static_cast<bool>(curr_start % 2),
        /*Set the color to the current iteration*/
        test_color_value);

    std::unique_ptr<DeskTemplate> desk_template =
        desk_template_conversion::FromSyncProto(desk_proto);

    WorkspaceDeskSpecifics converted_desk_proto =
        desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

    EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));

    // Shift range up by one.
    ++curr_start;
  }
}

TEST_F(DeskSyncBridgeTest, FloatingWorkspaceConversionShouldBeLossless) {
  CreateBridge();
  WorkspaceDeskSpecifics desk_proto =
      CreateFloatingWorkspaceTemplateExpectedValue("cache_guid");
  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(desk_proto);
  WorkspaceDeskSpecifics converted_desk_proto =
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache());

  std::unique_ptr<DeskTemplate> converted_desk_template =
      desk_template_conversion::FromSyncProto(converted_desk_proto);

  EXPECT_THAT(converted_desk_proto, EqualsSpecifics(desk_proto));
}

// Tests that URLs are saved properly when converting a DeskTemplate
// to its Protobuf form.
TEST_F(DeskSyncBridgeTest, EnsureAshBrowserWindowsSavedProperly) {
  CreateBridge();

  // Uses a different method to instantiate the template that doesn't rely
  // on the assumption that the template is instantiated from a proto, but
  // rather is captured and saved for the first time.
  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetUrls({GURL(base::StringPrintf(kTestUrlFormat, 1)),
                            GURL(base::StringPrintf(kTestUrlFormat, 2))})
                  .SetIsLacros(false)
                  .SetGenericBuilder(SavedDeskGenericAppBuilder().SetWindowId(
                      kBrowserWindowId))
                  .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreateBrowserTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time())));
}

TEST_F(DeskSyncBridgeTest, EnsureLacrosBrowserWindowsCanBeSavedProperly) {
  CreateBridge();

  // Uses a different method to instantiate the template that doesn't rely
  // on the assumption that the template is instantiated from a proto, but
  // rather is captured and saved for the first time.
  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetUrls({GURL(base::StringPrintf(kTestUrlFormat, 1)),
                            GURL(base::StringPrintf(kTestUrlFormat, 2))})
                  .SetIsLacros(true)
                  .SetGenericBuilder(SavedDeskGenericAppBuilder().SetWindowId(
                      kBrowserWindowId))
                  .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreateBrowserTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time())));
}

TEST_F(DeskSyncBridgeTest, EnsurePwaInAshChromeCanBeSavedProperly) {
  CreateBridge();

  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetUrls({GURL(base::StringPrintf(kTestUrlFormat, 1))})
                  .SetIsLacros(false)
                  .SetIsApp(true)
                  .SetGenericBuilder(
                      SavedDeskGenericAppBuilder().SetWindowId(kPwaWindowId))
                  .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreatePwaTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time())));
}

TEST_F(DeskSyncBridgeTest, EnsurePwaInLacrosChromeCanBeSavedProperly) {
  CreateBridge();

  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetUrls({GURL(base::StringPrintf(kTestUrlFormat, 1))})
                  .SetIsLacros(true)
                  .SetIsApp(true)
                  .SetGenericBuilder(
                      SavedDeskGenericAppBuilder().SetWindowId(kPwaWindowId))
                  .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreatePwaTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time())));
}

TEST_F(DeskSyncBridgeTest, EnsureChromeAppCanBeSavedProperly) {
  CreateBridge();

  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(SavedDeskGenericAppBuilder()
                            .SetAppId(desk_test_util::kTestChromeAppId)
                            .SetWindowId(kChromeAppWindowId)
                            .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreateChromeAppTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time(),
          kChromeAppWindowId, desk_test_util::kTestChromeAppId)));
}

TEST_F(DeskSyncBridgeTest, EnsureLacrosChromeAppCanBeSavedProperly) {
  CreateBridge();

  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(SavedDeskGenericAppBuilder()
                            .SetAppId(desk_test_util::kTestLacrosChromeAppId)
                            .SetWindowId(kChromeAppWindowId)
                            .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreateChromeAppTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time(),
          kChromeAppWindowId, desk_test_util::kTestLacrosChromeAppId)));
}

TEST_F(DeskSyncBridgeTest, EnsureUnsupportedAppCanBeIgnored) {
  constexpr int kUnsupportedAppWindowId = 1888;
  CreateBridge();

  std::unique_ptr<DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(base::StringPrintf(kUuidFormat, kDefaultTemplateIndex))
          .SetName(base::StringPrintf(kNameFormat, kDefaultTemplateIndex))
          .AddAppWindow(SavedDeskGenericAppBuilder()
                            .SetWindowId(kChromeAppWindowId)
                            .SetAppId(desk_test_util::kTestChromeAppId)
                            .Build())
          .AddAppWindow(SavedDeskGenericAppBuilder()
                            .SetWindowId(kUnsupportedAppWindowId)
                            .SetAppId(desk_test_util::kTestUnsupportedAppId)
                            .Build())
          .Build();

  EXPECT_THAT(
      desk_template_conversion::ToSyncProto(desk_template.get(), app_cache()),
      EqualsSpecifics(CreateChromeAppTemplateExpectedValue(
          kDefaultTemplateIndex, desk_template->created_time(),
          kChromeAppWindowId, desk_test_util::kTestChromeAppId)));
}

// Tests that the sync bridge appropriately handles explicitly unknown desk
// type as invalid.
TEST_F(DeskSyncBridgeTest, EnsureGracefulHandlingOfUnknownDeskTypes) {
  WorkspaceDeskSpecifics unknown_desk = CreateUnknownDeskType();
  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(unknown_desk);

  EXPECT_EQ(desk_template, nullptr);
}

// Tests that the sync bridge treat saved desk with missing desk type as desk
// template.
TEST_F(DeskSyncBridgeTest, EnsureHandlingOfMissingDeskTypes) {
  WorkspaceDeskSpecifics unknown_desk = CreateWorkspaceDeskWithoutDeskType();
  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(unknown_desk);

  EXPECT_EQ(desk_template->type(), DeskType::kTemplate);
}

TEST_F(DeskSyncBridgeTest, IsBridgeReady) {
  CreateBridge();
  ASSERT_FALSE(bridge()->IsReady());

  FinishInitialization();
  ASSERT_TRUE(bridge()->IsReady());
}

TEST_F(DeskSyncBridgeTest, IsBridgeSyncing) {
  InitializeBridge();
  ASSERT_TRUE(bridge()->IsSyncing());

  DisableBridgeSync();
  ASSERT_FALSE(bridge()->IsSyncing());
}

TEST_F(DeskSyncBridgeTest, InitializationWithLocalDataAndMetadata) {
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  DataTypeState state = StateWithEncryption("test_encryption_key");
  WriteToStoreWithMetadata({template1, template2}, state);
  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("test_encryption_key"),
                                /*entities=*/_)));

  InitializeBridge();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Verify both local specifics are loaded correctly.
  EXPECT_EQ(template1.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(
                    base::Uuid::ParseCaseInsensitive(template1.uuid())),
                app_cache())
                .SerializeAsString());

  EXPECT_EQ(template2.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(
                    base::Uuid::ParseCaseInsensitive(template2.uuid())),
                app_cache())
                .SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, GetAllEntriesIncludesPolicyEntries) {
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  DataTypeState state = StateWithEncryption("test_encryption_key");
  WriteToStoreWithMetadata({template1, template2}, state);
  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("test_encryption_key"),
                                /*entities=*/_)));

  InitializeBridge();

  bridge()->SetPolicyDeskTemplates(GetPolicyWithTwoTemplates());

  EXPECT_EQ(4ul, bridge()->GetAllEntryUuids().size());

  auto result = bridge()->GetAllEntries();
  EXPECT_EQ(result.status, DeskModel::GetAllEntriesStatus::kOk);
  EXPECT_EQ(result.entries.size(), 4ul);

  // Two of these templates should be from policy.
  EXPECT_EQ(base::ranges::count_if(result.entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            2l);

  bridge()->SetPolicyDeskTemplates("");
}

TEST_F(DeskSyncBridgeTest, AddEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  auto specifics1 = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1",
      AdvanceAndGetTime());
  auto specifics2 = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(2)).AsLowercaseString(), "template 2",
      AdvanceAndGetTime());

  base::RunLoop loop1;
  bridge()->AddOrUpdateEntry(
      desk_template_conversion::FromSyncProto(specifics1),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
            loop1.Quit();
          }));
  loop1.Run();

  base::RunLoop loop2;
  bridge()->AddOrUpdateEntry(
      desk_template_conversion::FromSyncProto(specifics2),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
            loop2.Quit();
          }));
  loop2.Run();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Verify the added desk template content.
  EXPECT_EQ(specifics1.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(1))),
                app_cache())
                .SerializeAsString());

  EXPECT_EQ(specifics2.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(2))),
                app_cache())
                .SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldFailWhenEntryIsTooLarge) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Create a large entry with 500 tabs. This entry should be too large for
  // Sync.
  constexpr int number_of_tabs = 500;
  auto specifics = ExampleWorkspaceDeskSpecifics(
      MakeTestUuid(TestUuidId(1)).AsLowercaseString(), "template 1",
      AdvanceAndGetTime(), number_of_tabs);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      desk_template_conversion::FromSyncProto(specifics),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status,
                      DeskModel::AddOrUpdateEntryStatus::kEntryTooLarge);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldSucceedWhenSyncIsDisabled) {
  InitializeBridge();
  DisableBridgeSync();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(
          MakeTestUuid(TestUuidId(1)), DeskTemplateSource::kUser, "template 1",
          AdvanceAndGetTime(), DeskTemplateType::kTemplate),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, AddEntryShouldFailWhenBridgeIsNotReady) {
  // Only create sync bridge but do not allow it to finish initialization.
  CreateBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  // Add entry should fail when the sync bridge is not ready.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(
          MakeTestUuid(TestUuidId(1)), DeskTemplateSource::kUser, "template 1",
          AdvanceAndGetTime(), DeskTemplateType::kTemplate),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kFailure);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, CanDetectDuplicateName) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  AddTwoTemplatesWithDuplicatedNames();

  // The two duplicated templates should be added.
  EXPECT_EQ(4ul, bridge()->GetAllEntryUuids().size());
  EXPECT_TRUE(bridge()->FindOtherEntryWithName(
      bridge()
          ->GetUserEntryByUUID(MakeTestUuid(TestUuidId(9)))
          ->template_name(),
      bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(9)))->type(),
      bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(9)))->uuid()));
}

TEST_F(DeskSyncBridgeTest, CanDetectNoDuplicateName) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  EXPECT_FALSE(bridge()->FindOtherEntryWithName(
      bridge()
          ->GetUserEntryByUUID(MakeTestUuid(TestUuidId(1)))
          ->template_name(),
      bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(1)))->type(),
      bridge()->GetUserEntryByUUID(MakeTestUuid(TestUuidId(1)))->uuid()));
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldSucceed) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  auto result = bridge()->GetEntryByUUID(MakeTestUuid(TestUuidId(1)));
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kOk);
  EXPECT_TRUE(result.entry);
}

TEST_F(DeskSyncBridgeTest, GetAllEntriesByUuidsReturnsCorrectSet) {
  InitializeBridge();

  AddTwoTemplates();

  std::set<base::Uuid> entry_uuids = bridge()->GetAllEntryUuids();

  EXPECT_EQ(2ul, entry_uuids.size());

  entry_uuids.erase(MakeTestUuid(TestUuidId(1)));
  entry_uuids.erase(MakeTestUuid(TestUuidId(2)));

  // IFF the set is correct it should be empty.
  EXPECT_TRUE(entry_uuids.empty());
}

// Verify that event_flag placeholder has been set. This is a short-term
// fix for https://crbug.com/1301798
TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldFillEventFlag) {
  InitializeBridge();

  AddTwoTemplates();

  auto result = bridge()->GetEntryByUUID(MakeTestUuid(TestUuidId(1)));
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kOk);
  EXPECT_TRUE(result.entry);
  for (const auto& [app_id, launch_list] :
       result.entry->desk_restore_data()->app_id_to_launch_list()) {
    for (const auto& [id, restore_data] : launch_list) {
      EXPECT_EQ(restore_data->event_flag, 0);
    }
  }
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldReturnAdminTemplate) {
  InitializeBridge();

  AddTwoTemplates();

  SetOneAdminTemplate();

  // There should be 3 templates: 2 user templates + 1 admin template.
  EXPECT_EQ(3ul, bridge()->GetAllEntryUuids().size());

  base::RunLoop loop;
  auto result = bridge()->GetEntryByUUID(MakeAdminTestUuid(TestUuidId(1)));
  EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, result.status);
  EXPECT_TRUE(result.entry);
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldFailWhenUuidIsNotFound) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  const base::Uuid nonExistingUuid =
      base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 5));

  base::RunLoop loop;
  auto result = bridge()->GetEntryByUUID(nonExistingUuid);
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kNotFound);
  EXPECT_FALSE(result.entry);
}

TEST_F(DeskSyncBridgeTest, GetEntryByUUIDShouldFailWhenUuidIsInvalid) {
  InitializeBridge();

  auto result = bridge()->GetEntryByUUID(base::Uuid());
  EXPECT_EQ(result.status, DeskModel::GetEntryByUuidStatus::kInvalidUuid);
  EXPECT_FALSE(result.entry);
}

TEST_F(DeskSyncBridgeTest, UpdateEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Update template 1
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(1);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(MakeTestUuid(TestUuidId(1)),
                                     DeskTemplateSource::kUser,
                                     "updated template 1", AdvanceAndGetTime(),
                                     DeskTemplateType::kTemplate),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
            loop.Quit();
          }));
  loop.Run();

  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
  // Template 1 should be updated.
  EXPECT_EQ(
      "updated template 1",
      base::UTF16ToUTF8(bridge()
                            ->GetUserEntryByUUID(MakeTestUuid(TestUuidId(1)))
                            ->template_name()));

  // Template 2 should be unchanged.
  EXPECT_EQ(
      "template 2",
      base::UTF16ToUTF8(bridge()
                            ->GetUserEntryByUUID(MakeTestUuid(TestUuidId(2)))
                            ->template_name()));
}

TEST_F(DeskSyncBridgeTest, DeleteEntryLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Delete template 1.
  base::RunLoop loop;
  bridge()->DeleteEntry(
      MakeTestUuid(TestUuidId(1)),
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // We should have only 1 template.
  EXPECT_EQ(1ul, bridge()->GetAllEntryUuids().size());
  // Template 2 should be unchanged.
  EXPECT_EQ(
      "template 2",
      base::UTF16ToUTF8(bridge()
                            ->GetUserEntryByUUID(MakeTestUuid(TestUuidId(2)))
                            ->template_name()));
}

TEST_F(DeskSyncBridgeTest, DeleteAllEntriesLocally) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());

  // Seed two templates.
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Delete all templates.
  base::RunLoop loop;
  bridge()->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
        loop.Quit();
      }));
  loop.Run();

  // We should have no templates.
  EXPECT_EQ(0ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplyIncrementalSyncChangesEmpty) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(_)).Times(0);
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, ApplyIncrementalSyncChangesWithTwoAdditions) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({template1, template2}));
  EXPECT_FALSE(error);

  // We should have two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, ApplyIncrementalSyncChangesWithOneUpdate) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({template1, template2}));

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Now update template 1 with a new content.
  WorkspaceDeskSpecifics updated_template1 = CreateWorkspaceDeskSpecifics(1);
  updated_template1.set_name("updated template 1");

  EntityChangeList update_changes;
  update_changes.push_back(EntityChange::CreateUpdate(
      template1.uuid(), MakeEntityData(updated_template1)));

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(1)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(update_changes));
  // We should still have both templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
  // Template 1 should be updated to new content.
  EXPECT_EQ(updated_template1.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(
                    base::Uuid::ParseCaseInsensitive(template1.uuid())),
                app_cache())
                .SerializeAsString());
  EXPECT_EQ(template2.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(
                    base::Uuid::ParseCaseInsensitive(template2.uuid())),
                app_cache())
                .SerializeAsString());
}

// Tests that remote desk template can be correctly removed.
TEST_F(DeskSyncBridgeTest, ApplyIncrementalSyncChangesWithOneDeletion) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        EntityAddList({template1, template2}));

  // Verify that we have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Now delete template 1.
  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(template1.uuid()));

  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(SizeIs(1)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(delete_changes));

  // Verify that we only have template 2.
  EXPECT_EQ(1ul, bridge()->GetAllEntryUuids().size());
  EXPECT_EQ(template2.SerializeAsString(),
            desk_template_conversion::ToSyncProto(
                bridge()->GetUserEntryByUUID(
                    base::Uuid::ParseCaseInsensitive(template2.uuid())),
                app_cache())
                .SerializeAsString());
}

TEST_F(DeskSyncBridgeTest, ApplyIncrementalSyncChangesDeleteNonexistent) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(_)).Times(0);

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  EXPECT_CALL(*processor(), Delete).Times(0);

  EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("no-such-uuid"));
  auto error = bridge()->ApplyIncrementalSyncChanges(
      std::move(metadata_changes), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(DeskSyncBridgeTest, MergeFullSyncDataWithTwoEntries) {
  InitializeBridge();

  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(1);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(2);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));
  bridge()->MergeFullSyncData(std::move(metadata_change_list),
                              EntityAddList({template1, template2}));
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest, MergeFullSyncDataUploadsLocalOnlyEntries) {
  InitializeBridge();

  // Seed two templates.
  // Seeded templates will be "template 1" and "template 2".
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  // Create server-side templates "template 2" and "template 3".
  const WorkspaceDeskSpecifics template1 = CreateWorkspaceDeskSpecifics(2);
  const WorkspaceDeskSpecifics template2 = CreateWorkspaceDeskSpecifics(3);

  auto metadata_change_list = std::make_unique<InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedOrUpdatedRemotely(SizeIs(2)));

  // MergeFullSyncData should upload the local-only template "template 1".
  EXPECT_CALL(*processor(),
              Put(StrEq(MakeTestUuid(TestUuidId(1)).AsLowercaseString()), _, _))
      .Times(1);

  bridge()->MergeFullSyncData(std::move(metadata_change_list),
                              EntityAddList({template1, template2}));

  // Merged data should contain 3 templtes.
  EXPECT_EQ(3ul, bridge()->GetAllEntryUuids().size());
}

TEST_F(DeskSyncBridgeTest,
       GetEntryCountShouldIncludeBothUserAndAdminTemplates) {
  InitializeBridge();

  AddTwoTemplates();

  SetOneAdminTemplate();

  // There should be 3 templates: 2 user templates + 1 admin template.
  EXPECT_EQ(3ul, bridge()->GetEntryCount());
}

TEST_F(DeskSyncBridgeTest, GetMaxEntryCountShouldIncreaseWithAdminTemplates) {
  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(6ul, bridge()->GetMaxDeskTemplateEntryCount());
  SetOneAdminTemplate();

  // The max entry count should increase by 1 since we have set an admin
  // template.
  EXPECT_EQ(7ul, bridge()->GetMaxDeskTemplateEntryCount());
}

TEST_F(DeskSyncBridgeTest, GetTemplateJsonShouldReturnList) {
  InitializeBridge();

  // Seed two templates.
  // Seeded templates will be "template 1" and "template 2".
  AddTwoTemplates();

  // We should have seeded two templates.
  EXPECT_EQ(2ul, bridge()->GetAllEntryUuids().size());

  base::RunLoop loop;
  bridge()->GetTemplateJson(
      MakeTestUuid(TestUuidId(1)), app_cache(),
      base::BindLambdaForTesting([&](DeskModel::GetTemplateJsonStatus status,
                                     const base::Value& templates_json) {
        EXPECT_EQ(DeskModel::GetTemplateJsonStatus::kOk, status);

        EXPECT_FALSE(templates_json.is_none());

        std::string template_json_string;
        bool parsed_result =
            base::JSONWriter::Write(templates_json, &template_json_string);

        EXPECT_TRUE(parsed_result);

        // Content of the conversion is tested in:
        // components/desks_storage/core/desk_template_conversion_unittests.cc
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, AddUnknownDeskTypeShouldFail) {
  InitializeBridge();

  WorkspaceDeskSpecifics unknown_desk = CreateUnknownDeskType();
  std::unique_ptr<DeskTemplate> desk_template =
      desk_template_conversion::FromSyncProto(unknown_desk);

  base::RunLoop loop;
  bridge()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(status,
                      DeskModel::AddOrUpdateEntryStatus::kInvalidArgument);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(DeskSyncBridgeTest, CanRecordFileSizeMetrics) {
  base::HistogramTester histogram_tester;

  InitializeBridge();

  AddTwoTemplates();

  EXPECT_EQ(2ul, bridge()->GetEntryCount());

  histogram_tester.ExpectTotalCount(kTemplateSizeHistogramName, 2u);
  histogram_tester.ExpectBucketCount(kTemplateSizeHistogramName, 572, 2u);
}

}  // namespace

}  // namespace desks_storage
