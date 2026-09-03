// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/api_mock_setup_mixin.h"

#include <map>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "google_apis/gaia/gaia_switches.h"

namespace supervised_user {

namespace {

// Waits until the browser is in intended state. Specifically, for supervised
// user, either waits for the family member to be loaded (by inspecting if a
// preference holding its email has required value), otherwise is an no-op.
void WaitUntilReady(InProcessBrowserTest* test_base,
                    std::string_view preference,
                    std::string_view value) {
#if BUILDFLAG(IS_CHROMEOS)
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
#else
  PrefService* pref_service = test_base->browser()->GetProfile()->GetPrefs();
#endif

  if (pref_service->GetString(prefs::kSupervisedUserId) !=
      supervised_user::kChildAccountSUID) {
    return;
  }

  PrefChangeRegistrar registrar;
  registrar.Init(pref_service);

  base::RunLoop run_loop;
  registrar.Add(std::string(preference), base::BindLambdaForTesting([&]() {
                  CHECK_EQ(pref_service->GetString(preference), value)
                      << "Unexpected family member preference value.";
                  run_loop.Quit();
                }));

  if (pref_service->GetString(preference) == value) {
    return;
  }

  run_loop.Run();
}
}  // namespace

KidsManagementApiMockSetupMixin::KidsManagementApiMockSetupMixin(
    InProcessBrowserTestMixinHost& host,
    InProcessBrowserTest* test_base)
    : InProcessBrowserTestMixin(&host), test_base_(test_base) {}
KidsManagementApiMockSetupMixin::~KidsManagementApiMockSetupMixin() = default;

void KidsManagementApiMockSetupMixin::SetUp() {
  api_mock_.InstallOn(embedded_test_server_);
  CHECK(embedded_test_server_.InitializeAndListen());
}

void KidsManagementApiMockSetupMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  base::DictValue config_dict;
  if (command_line->HasSwitch(switches::kGaiaConfigContents)) {
    std::optional<base::DictValue> existing_dict = base::JSONReader::ReadDict(
        command_line->GetSwitchValueASCII(switches::kGaiaConfigContents),
        base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (existing_dict) {
      config_dict = std::move(*existing_dict);
    }
    command_line->RemoveSwitch(switches::kGaiaConfigContents);
  }
  base::DictValue* urls_dict = config_dict.EnsureDict("urls");
  urls_dict->Set(
      "kids_management_api_origin_url",
      base::DictValue().Set("url", embedded_test_server_.base_url().spec()));
  command_line->AppendSwitchASCII(switches::kGaiaConfigContents,
                                  base::WriteJson(config_dict).value_or(""));
}

void KidsManagementApiMockSetupMixin::SetUpOnMainThread() {
  embedded_test_server_.StartAcceptingConnections();
  CHECK_EQ(kSimpsonFamily.count(kidsmanagement::HEAD_OF_HOUSEHOLD),
           std::size_t(1))
      << "Expected single head of household";

  WaitUntilReady(
      test_base_, prefs::kSupervisedUserCustodianName,
      kSimpsonFamily.find(kidsmanagement::HEAD_OF_HOUSEHOLD)->second);
}

void KidsManagementApiMockSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_.ShutdownAndWaitUntilComplete());
}

}  // namespace supervised_user
