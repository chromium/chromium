// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/keystone/ksadmin.h"

#include <stdio.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/mac/mac_util.h"
#include "chrome/updater/mac/setup/ks_tickets.h"
#include "chrome/updater/mac/update_service_proxy.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"

namespace updater {
namespace {

constexpr char kCommandDelete[] = "delete";
constexpr char kCommandInstall[] = "install";
constexpr char kCommandList[] = "list";
constexpr char kCommandKsadminVersion[] = "ksadmin-version";
constexpr char kCommandPrintTag[] = "print-tag";
constexpr char kCommandPrintTickets[] = "print-tickets";
constexpr char kCommandRegister[] = "register";
constexpr char kCommandSystemStore[] = "system-store";
constexpr char kCommandUserInitiated[] = "user-initiated";
constexpr char kCommandUserStore[] = "user-store";
constexpr char kCommandBrandKey[] = "brand-key";
constexpr char kCommandBrandPath[] = "brand-path";
constexpr char kCommandProductId[] = "productid";
constexpr char kCommandTag[] = "tag";
constexpr char kCommandTagKey[] = "tag-key";
constexpr char kCommandTagPath[] = "tag-path";
constexpr char kCommandVersion[] = "version";
constexpr char kCommandVersionKey[] = "version-key";
constexpr char kCommandVersionPath[] = "version-path";
constexpr char kCommandXCPath[] = "xcpath";

// base::CommandLine can't be used because it enforces that all switches are
// lowercase, but ksadmin has case-sensitive switches. This argument parser
// converts an argv set into a map of switch name to switch value; for example
// `ksadmin --register --productid com.goog.chrome -v 1.2.3.4 e` to
// `{"register": "", "productid": "com.goog.chrome", "v": "1.2.3.4", "e": ""}`.
base::flat_map<std::string, std::string> ParseCommandLine(int argc,
                                                          char* argv[]) {
  base::flat_map<std::string, std::string> result;
  std::string last_arg;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    bool new_key = last_arg.empty();
    for (const std::string& prefix : {"--", "-"}) {
      if (base::StartsWith(arg, prefix)) {
        new_key = true;
        arg = arg.substr(prefix.length());
        break;
      }
    }
    if (new_key) {
      result[last_arg] = "";
      last_arg = arg;
    } else {
      result[last_arg] = arg;
      last_arg = "";
    }
  }
  if (!last_arg.empty())
    result[last_arg] = "";
  return result;
}

bool HasSwitch(const std::string& arg,
               const base::flat_map<std::string, std::string>& switches) {
  if (base::Contains(switches, arg))
    return true;
  const static base::flat_map<std::string, std::vector<std::string>> aliases = {
      {kCommandDelete, {"d"}},        {kCommandInstall, {"i"}},
      {kCommandList, {"l"}},          {kCommandKsadminVersion, {"k"}},
      {kCommandPrintTag, {"G"}},      {kCommandPrintTickets, {"print", "p"}},
      {kCommandRegister, {"r"}},      {kCommandSystemStore, {"S"}},
      {kCommandUserInitiated, {"F"}}, {kCommandUserStore, {"U"}},
  };
  if (!base::Contains(aliases, arg))
    return false;
  for (const auto& alias : aliases.at(arg)) {
    if (base::Contains(switches, alias))
      return true;
  }
  return false;
}

std::string SwitchValue(
    const std::string& arg,
    const base::flat_map<std::string, std::string>& switches) {
  if (base::Contains(switches, arg))
    return switches.at(arg);
  const static base::flat_map<std::string, std::string> aliases = {
      {kCommandBrandKey, "b"},    {kCommandBrandPath, "B"},
      {kCommandProductId, "P"},   {kCommandTag, "g"},
      {kCommandTagKey, "K"},      {kCommandTagPath, "H"},
      {kCommandVersion, "v"},     {kCommandVersionKey, "e"},
      {kCommandVersionPath, "a"}, {kCommandXCPath, "x"},
  };
  if (!base::Contains(aliases, arg))
    return "";
  const std::string& alias = aliases.at(arg);
  return base::Contains(switches, alias) ? switches.at(alias) : "";
}

std::string KeystoneTicketStorePath(UpdaterScope scope) {
  return GetKeystoneFolderPath(scope)
      ->Append(FILE_PATH_LITERAL("TicketStore"))
      .Append(FILE_PATH_LITERAL("Keystone.ticketstore"))
      .value();
}

UpdaterScope Scope(const base::flat_map<std::string, std::string>& switches) {
  if (HasSwitch(kCommandSystemStore, switches))
    return UpdaterScope::kSystem;
  if (HasSwitch(kCommandUserStore, switches))
    return UpdaterScope::kUser;

  base::FilePath executable_path;
  if (!base::PathService::Get(base::FILE_EXE, &executable_path))
    return UpdaterScope::kUser;

  return base::StartsWith(executable_path.value(),
                          GetKeystoneFolderPath(UpdaterScope::kSystem)->value())
             ? UpdaterScope::kSystem
             : UpdaterScope::kUser;
}

class KSAdminApp : public App {
 public:
  explicit KSAdminApp(const base::flat_map<std::string, std::string>& switches)
      : switches_(switches),
        service_proxy_(base::MakeRefCounted<UpdateServiceProxy>(Scope())) {}

 private:
  ~KSAdminApp() override = default;
  void FirstTaskRun() override;

  // Command handlers; each will eventually call Shutdown.
  void CheckForUpdates();
  void Register();
  void Delete();
  void PrintTag();
  int PrintKeystoneTag(const std::string& app_id);
  void PrintUsage(const std::string& error_message);
  void PrintVersion();
  void PrintTickets();
  void PrintKeystoneTickets(const std::string& app_id);

  UpdaterScope Scope() const;
  bool HasSwitch(const std::string& arg) const;
  std::string SwitchValue(const std::string& arg) const;

  NSDictionary<NSString*, KSTicket*>* LoadTicketStore();

  const base::flat_map<std::string, std::string> switches_;
  scoped_refptr<UpdateServiceProxy> service_proxy_;
};

void KSAdminApp::PrintUsage(const std::string& error_message) {
  if (!error_message.empty())
    LOG(ERROR) << error_message;
  const std::string usage_message =
      "Usage: ksadmin [action...] [option...]\n"
      "Actions:\n"
      "  --delete,-d          Delete a ticket. Use with -P.\n"
      "  --install,-i         Check for and apply updates.\n"
      "  --ksadmin-version,-k Print the version of ksadmin.\n"
      "  --print              An alias for --print-tickets.\n"
      "  --print-tag,-G       Print a ticket's tag. Use with -P.\n"
      "  --print-tickets,-p   Print all tickets. Can filter with -P.\n"
      "  --register,-r        Register a new ticket. Use with -P, -v, -x,\n"
      "                       -e, -a, -K, -H, -g.\n"
      "Action parameters:\n"
      "  --brand-key,-b      Set the brand code key. Use with -P and -B.\n"
      "                      Value must be empty or KSBrandID.\n"
      "  --brand-path,-B     Set the brand code path. Use with -P and -b.\n"
      "  --productid,-P id   ProductID.\n"
      "  --system-store,-S   Use the system-wide ticket store.\n"
      "  --tag,-g TAG        Set the tag. Use with -P.\n"
      "  --tag-key,-K        Set the tag path key. Use with -P and -H.\n"
      "  --tag-path,-H       Set the tag path. Use with -P and -K.\n"
      "  --user-initiated,-F This operation is initiated by a user.\n"
      "  --user-store,-U     Use a per-user ticket store.\n"
      "  --version,-v VERS   Set the version. Use with -P.\n"
      "  --version-key,-e    Set the version path key. Use with -P and -a.\n"
      "  --version-path,-a   Set the version path. Use with -P and -e.\n"
      "  --xcpath,-x PATH    Set a path to use as an existence checker.\n";
  printf("%s\n", usage_message.c_str());
  Shutdown(error_message.empty() ? 0 : 1);
}

void KSAdminApp::Register() {
  RegistrationRequest registration;
  registration.app_id = SwitchValue(kCommandProductId);
  registration.ap = SwitchValue(kCommandTag);
  registration.version = base::Version(SwitchValue(kCommandVersion));
  registration.existence_checker_path =
      base::FilePath(SwitchValue(kCommandXCPath));

  const std::string brand_key = SwitchValue(kCommandBrandKey);
  if (brand_key.empty() ||
      brand_key == base::SysNSStringToUTF8(kCRUTicketBrandKey)) {
    registration.brand_path = base::FilePath(SwitchValue(kCommandBrandPath));
  } else {
    PrintUsage("Unsupported brand key.");
    return;
  }

  if (registration.app_id.empty() || !registration.version.IsValid()) {
    PrintUsage("Registration information invalid.");
    return;
  }

  service_proxy_->RegisterApp(
      registration, base::BindOnce(
                        [](base::OnceCallback<void(int)> cb,
                           const RegistrationResponse& response) {
                          if (response.status_code == kRegistrationSuccess) {
                            std::move(cb).Run(0);
                          } else {
                            LOG(ERROR) << "Updater registration error: "
                                       << response.status_code;
                            std::move(cb).Run(1);
                          }
                        },
                        base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::CheckForUpdates() {
  std::string app_id = SwitchValue(kCommandProductId);
  if (app_id.empty()) {
    PrintUsage("productid missing");
    return;
  }

  service_proxy_->Update(
      app_id,
      HasSwitch(kCommandUserInitiated) ? UpdateService::Priority::kForeground
                                       : UpdateService::Priority::kBackground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating([](const UpdateService::UpdateState& update_state) {
        if (update_state.state == UpdateService::UpdateState::State::kUpdated) {
          printf("Finished updating (errors=%d reboot=%s)\n", 0, "YES");
        }
      }),
      base::BindOnce(
          [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            if (result == UpdateService::Result::kSuccess) {
              printf("Available updates: (\n)\n");
              std::move(cb).Run(0);
            } else {
              LOG(ERROR) << "Error code: " << result;
              std::move(cb).Run(1);
            }
          },
          base::BindOnce(&KSAdminApp::Shutdown, this)));
}

bool KSAdminApp::HasSwitch(const std::string& arg) const {
  return updater::HasSwitch(arg, switches_);
}

std::string KSAdminApp::SwitchValue(const std::string& arg) const {
  return updater::SwitchValue(arg, switches_);
}

UpdaterScope KSAdminApp::Scope() const {
  return updater::Scope(switches_);
}

void KSAdminApp::Delete() {
  // TODO(crbug.com/1250524): Implement.
  Shutdown(1);
}

NSDictionary<NSString*, KSTicket*>* KSAdminApp::LoadTicketStore() {
  return
      [KSTicketStore readStoreWithPath:base::SysUTF8ToNSString(
                                           KeystoneTicketStorePath(Scope()))];
}

int KSAdminApp::PrintKeystoneTag(const std::string& app_id) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = LoadTicketStore();
    KSTicket* ticket =
        [store objectForKey:[base::SysUTF8ToNSString(app_id) lowercaseString]];
    if (ticket) {
      printf("%s\n", base::SysNSStringToUTF8([ticket determineTag]).c_str());
    } else {
      printf("No ticket for %s\n", app_id.c_str());
      return 1;
    }
  }
  return 0;
}

void KSAdminApp::PrintTag() {
  const std::string app_id = SwitchValue(kCommandProductId);
  if (app_id.empty()) {
    PrintUsage("productid missing");
    return;
  }

  service_proxy_->GetAppStates(base::BindOnce(
      [](const std::string& app_id,
         base::OnceCallback<int(const std::string&)> fallback_cb,
         base::OnceCallback<void(int)> done_cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        int exit_code = 0;

        std::vector<updater::UpdateService::AppState>::const_iterator it =
            std::find_if(
                std::begin(states), std::end(states),
                [&app_id](const updater::UpdateService::AppState& state) {
                  return base::EqualsCaseInsensitiveASCII(state.app_id, app_id);
                });
        if (it != std::end(states)) {
          KSTicket* ticket =
              [[[KSTicket alloc] initWithAppState:*it] autorelease];
          printf("%s\n",
                 base::SysNSStringToUTF8([ticket determineTag]).c_str());

        } else {
          // Fallback to print tag from legacy Keystone tickets if there's no
          // matching app registered with the Chromium updater.
          exit_code = std::move(fallback_cb).Run(app_id);
        }

        std::move(done_cb).Run(exit_code);
      },
      app_id, base::BindOnce(&KSAdminApp::PrintKeystoneTag, this),
      base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::PrintVersion() {
  printf("%s\n", kUpdaterVersion);
  Shutdown(0);
}

void KSAdminApp::PrintKeystoneTickets(const std::string& app_id) {
  // Print all tickets if `app_id` is empty. Otherwise only print ticket for
  // the given app id.
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = LoadTicketStore();
    if (app_id.empty()) {
      if (store.count > 0) {
        for (NSString* key in store) {
          printf("%s\n",
                 base::SysNSStringToUTF8([store[key] description]).c_str());
        }
        return;
      }
    } else {
      KSTicket* ticket = [store
          objectForKey:[base::SysUTF8ToNSString(app_id) lowercaseString]];
      if (ticket) {
        printf("%s\n", base::SysNSStringToUTF8([ticket description]).c_str());
        return;
      }
    }

    printf("No tickets.\n");
  }
}

void KSAdminApp::PrintTickets() {
  const std::string app_id = SwitchValue(kCommandProductId);
  service_proxy_->GetAppStates(base::BindOnce(
      [](const std::string& app_id, base::OnceCallback<void()> fallback_cb,
         base::OnceCallback<void(int)> done_cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        for (const updater::UpdateService::AppState& state : states) {
          if (!app_id.empty() &&
              !base::EqualsCaseInsensitiveASCII(app_id, state.app_id)) {
            continue;
          }
          KSTicket* ticket =
              [[[KSTicket alloc] initWithAppState:state] autorelease];
          printf("%s\n", base::SysNSStringToUTF8([ticket description]).c_str());
        }

        // Fallback to print legacy Keystone tickets if there's no app
        // registered with the new updater.
        if (states.empty()) {
          std::move(fallback_cb).Run();
        }
        std::move(done_cb).Run(0);
      },
      app_id, base::BindOnce(&KSAdminApp::PrintKeystoneTickets, this, app_id),
      base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::FirstTaskRun() {
  if ((geteuid() == 0) && (getuid() != 0)) {
    if (setuid(0) || setgid(0)) {
      LOG(ERROR) << "Can't setuid()/setgid() appropriately.";
      Shutdown(1);
    }
  }
  const base::flat_map<std::string, void (KSAdminApp::*)()> commands = {
      {kCommandDelete, &KSAdminApp::Delete},
      {kCommandInstall, &KSAdminApp::CheckForUpdates},
      {kCommandList, &KSAdminApp::CheckForUpdates},
      {kCommandKsadminVersion, &KSAdminApp::PrintVersion},
      {kCommandPrintTag, &KSAdminApp::PrintTag},
      {kCommandPrintTickets, &KSAdminApp::PrintTickets},
      {kCommandRegister, &KSAdminApp::Register},
  };
  for (const auto& entry : commands) {
    if (HasSwitch(entry.first)) {
      (this->*entry.second)();
      return;
    }
  }
  PrintUsage("");
}

}  // namespace

int KSAdminAppMain(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::flat_map<std::string, std::string> command_line =
      ParseCommandLine(argc, argv);
  updater::InitLogging(Scope(command_line), FILE_PATH_LITERAL("updater.log"));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return base::MakeRefCounted<KSAdminApp>(command_line)->Run();
}

}  // namespace updater
