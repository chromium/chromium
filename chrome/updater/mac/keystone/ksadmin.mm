// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/mac/keystone/ksadmin.h"

#include <stdio.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/at_exit.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/mac/setup/ks_tickets.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/util.h"

namespace updater {

// base::CommandLine can't be used because it enforces that all switches are
// lowercase, but ksadmin has case-sensitive switches. This argument parser
// converts an argv set into a map of switch name to switch value; for example
// `ksadmin --register --productid=com.goog.chrome -v 1.2.3.4 e` to
// `{"register": "", "productid": "com.goog.chrome", "v": "1.2.3.4", "e": ""}`.
std::map<std::string, std::string> ParseCommandLine(int argc,
                                                    const char* argv[]) {
  std::map<std::string, std::string> result;
  std::string key;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (base::StartsWith(arg, "--")) {
      key = arg.substr(2);
      size_t eq_idx = key.find('=');
      if (eq_idx == std::string::npos) {
        result[key] = "";
      } else {
        result[key.substr(0, eq_idx)] = key.substr(eq_idx + 1);
        key = "";
      }
    } else if (base::StartsWith(arg, "-")) {
      // Multiple short options could be combined together. For example,
      // command `ksadmin -pP com.google.Chrome` should print Chrome ticket.
      // Split the option substring into switches character by character.
      for (const char ch : arg.substr(1)) {
        if (ch == '=') {
          size_t eq_idx = arg.find('=', 1);
          CHECK_NE(eq_idx, std::string::npos)
              << "after reaching '=' in short option \"" << arg
              << "\", could not find it. This can't happen.";
          result[key] = arg.substr(eq_idx + 1);
          key = "";
          break;  // do not process value as additional short options
        }
        key = ch;
        result[key] = "";
      }
    } else {
      if (!key.empty()) {
        result[key] = arg;
      }
      key = "";
    }
  }
  return result;
}

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
constexpr char kCommandStorePath[] = "store";
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

bool HasSwitch(const std::string& arg,
               const std::map<std::string, std::string>& switches) {
  if (switches.contains(arg)) {
    return true;
  }
  static const base::NoDestructor<
      std::map<std::string, std::vector<std::string>>>
      aliases{{
          {kCommandDelete, {"d"}},
          {kCommandInstall, {"i"}},
          {kCommandList, {"l"}},
          {kCommandKsadminVersion, {"k"}},
          {kCommandPrintTag, {"G"}},
          {kCommandPrintTickets, {"print", "p"}},
          {kCommandRegister, {"r"}},
          {kCommandSystemStore, {"S"}},
          {kCommandUserInitiated, {"F"}},
          {kCommandUserStore, {"U"}},
      }};
  if (!aliases->contains(arg)) {
    return false;
  }
  for (const auto& alias : aliases->at(arg)) {
    if (switches.contains(alias)) {
      return true;
    }
  }
  return false;
}

std::string SwitchValue(const std::string& arg,
                        const std::map<std::string, std::string>& switches) {
  if (switches.contains(arg)) {
    return switches.at(arg);
  }
  static constexpr auto kAliases =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{kCommandBrandKey, "b"},
           {kCommandBrandPath, "B"},
           {kCommandProductId, "P"},
           {kCommandTag, "g"},
           {kCommandTagKey, "K"},
           {kCommandTagPath, "H"},
           {kCommandVersion, "v"},
           {kCommandVersionKey, "e"},
           {kCommandVersionPath, "a"},
           {kCommandXCPath, "x"}});
  if (!kAliases.contains(arg)) {
    return "";
  }
  const std::string alias{kAliases.at(arg)};
  return switches.contains(alias) ? switches.at(alias) : "";
}

std::string KeystoneTicketStorePath(UpdaterScope scope) {
  return GetKeystoneFolderPath(scope)
      ->Append(FILE_PATH_LITERAL("TicketStore"))
      .Append(FILE_PATH_LITERAL("Keystone.ticketstore"))
      .value();
}

bool IsSystemShim() {
  base::FilePath executable_path;
  if (!base::PathService::Get(base::FILE_EXE, &executable_path)) {
    return false;
  }

  return base::StartsWith(
      executable_path.value(),
      GetKeystoneFolderPath(UpdaterScope::kSystem)->value());
}

UpdaterScope Scope(const std::map<std::string, std::string>& switches) {
  if (HasSwitch(kCommandSystemStore, switches)) {
    return UpdaterScope::kSystem;
  }
  if (HasSwitch(kCommandUserStore, switches)) {
    return UpdaterScope::kUser;
  }

  if (HasSwitch(kCommandStorePath, switches)) {
    return SwitchValue(kCommandStorePath, switches) ==
                   KeystoneTicketStorePath(UpdaterScope::kSystem)
               ? UpdaterScope::kSystem
               : UpdaterScope::kUser;
  }
  return IsSystemShim() ? UpdaterScope::kSystem : UpdaterScope::kUser;
}

class UpdateCheckResult : public base::RefCountedThreadSafe<UpdateCheckResult> {
 public:
  explicit UpdateCheckResult(const std::string& app_id) : app_id_(app_id) {}

  std::string app_id() const { return app_id_; }
  std::string next_version() const { return next_version_; }

  void set_next_version(const std::string& next_version) {
    next_version_ = next_version;
  }

 protected:
  virtual ~UpdateCheckResult() = default;

 private:
  friend class base::RefCountedThreadSafe<UpdateCheckResult>;

  const std::string app_id_;

  // The version that the app can currently upgrade to.
  std::string next_version_;
};

class KSAdminApp : public App {
 public:
  explicit KSAdminApp(const std::map<std::string, std::string>& switches)
      : switches_(switches),
        system_service_proxy_(CreateUpdateServiceProxy(UpdaterScope::kSystem)),
        user_service_proxy_(CreateUpdateServiceProxy(UpdaterScope::kUser)) {}

 private:
  ~KSAdminApp() override = default;
  void FirstTaskRun() override;

  // Command handlers; each will eventually call Shutdown.
  void UpdateApp();
  void ListAppUpdate();
  void Register();
  void Delete();
  void PrintTag();
  void PrintUsage(const std::string& error_message);
  void PrintVersion();
  void PrintTickets();

  void DoUpdateApp(UpdaterScope scope);
  void DoListAppUpdate(UpdaterScope scope);
  void DoPrintTag(UpdaterScope scope);
  void DoPrintTickets(UpdaterScope scope);

  static KSTicket* TicketFromAppState(
      const updater::UpdateService::AppState& state);
  int PrintKeystoneTag(UpdaterScope scope, const std::string& app_id) const;
  bool PrintKeystoneTickets(UpdaterScope scope,
                            const std::string& app_id) const;

  scoped_refptr<UpdateService> ServiceProxy(UpdaterScope scope) const;
  bool MatchesXCPath(NSString* ticket_path) const;
  void ChooseService(base::OnceCallback<void(UpdaterScope scope)> callback);
  void FinishChoosingServiceWithSystemAppStates(
      base::OnceCallback<void(UpdaterScope)> callback,
      const std::vector<updater::UpdateService::AppState>& states) const;
  void MaybeInstallUpdater(UpdaterScope scope) const;

  bool HasSwitch(const std::string& arg) const;
  std::string SwitchValue(const std::string& arg) const;

  NSDictionary<NSString*, KSTicket*>* LoadTicketStore(UpdaterScope) const;

  const std::map<std::string, std::string> switches_;
  scoped_refptr<UpdateService> system_service_proxy_;
  scoped_refptr<UpdateService> user_service_proxy_;
  ScopedIPCSupportWrapper ipc_support_;
};

KSTicket* KSAdminApp::TicketFromAppState(
    const updater::UpdateService::AppState& state) {
  return [[KSTicket alloc]
      initWithAppId:base::SysUTF8ToNSString(state.app_id)
            version:base::SysUTF8ToNSString(state.version.GetString())
                ecp:state.ecp
                tag:base::SysUTF8ToNSString(state.ap)
          brandCode:base::SysUTF8ToNSString(state.brand_code)
          brandPath:state.brand_path];
}

scoped_refptr<UpdateService> KSAdminApp::ServiceProxy(
    UpdaterScope scope) const {
  return IsSystemInstall(scope) ? system_service_proxy_ : user_service_proxy_;
}

void KSAdminApp::MaybeInstallUpdater(UpdaterScope scope) const {
  const std::optional<base::FilePath> path = GetUpdaterExecutablePath(scope);

  if (path &&
      [NSFileManager.defaultManager
          fileExistsAtPath:base::apple::FilePathToNSString(path.value())]) {
    return;
  }

  if (!HasSwitch(kCommandInstall) && !HasSwitch(kCommandRegister)) {
    return;
  }

  if (IsSystemInstall(scope) && geteuid() != 0) {
    VLOG(0) << "Cannot install system updater without root privilege.";
    return;
  }

  const std::optional<base::FilePath> setup_path = GetUpdaterExecutablePath(
      IsSystemShim() ? UpdaterScope::kSystem : UpdaterScope::kUser);
  if (!setup_path || ![NSFileManager.defaultManager
                         fileExistsAtPath:base::apple::FilePathToNSString(
                                              setup_path.value())]) {
    VLOG(0) << "No existing updater to install from.";
    return;
  }

  base::CommandLine install_command(setup_path.value());
  install_command.AppendSwitch(kInstallSwitch);
  if (IsSystemInstall(scope)) {
    install_command.AppendSwitch(kSystemSwitch);
  }
  int exit_code = -1;
  const base::Process process = base::LaunchProcess(install_command, {});
  if (process.IsValid() && process.WaitForExit(&exit_code)) {
    VLOG(0) << "Installer returned " << exit_code << ".";
  } else {
    VLOG(0) << "Failed to wait for the installer to exit.";
  }
}

bool KSAdminApp::MatchesXCPath(NSString* ticket_path) const {
  if (!ticket_path.length) {
    return true;
  }
  std::string xcpath_raw = SwitchValue(kCommandXCPath);
  if (xcpath_raw.empty()) {
    return true;
  }

  @autoreleasepool {
    NSString* xcpath = base::SysUTF8ToNSString(xcpath_raw);
    xcpath = [xcpath stringByStandardizingPath];
    ticket_path = [ticket_path stringByStandardizingPath];
    return static_cast<bool>([xcpath isEqual:ticket_path]);
  }
}

void KSAdminApp::ChooseService(
    base::OnceCallback<void(UpdaterScope)> callback) {
  // Choose updater in the following order:
  //   1. If user explicitly specified the scope (based on `-S` or `-U` or
  //      value of `--store`).
  //   2. Choose system updater if user is root.
  //   3. Prefer system updater if app ID is given and it matches a system app.
  //      If a path hint is provided, compare it to the app's existence
  //      checker path to determine whether this really is the same app.
  //   4. Otherwise choose user updater.
  //
  // If ksadmin is running as `root` and deduces the user updater, this logs
  // an error and shuts the process down with exit code 1.
  std::optional<UpdaterScope> scope = std::nullopt;
  if (HasSwitch(kCommandSystemStore)) {
    scope = std::make_optional(UpdaterScope::kSystem);
  } else if (HasSwitch(kCommandUserStore)) {
    scope = std::make_optional(UpdaterScope::kUser);
  } else if (HasSwitch(kCommandStorePath)) {
    scope = std::make_optional(
        SwitchValue(kCommandStorePath) ==
                KeystoneTicketStorePath(UpdaterScope::kSystem)
            ? UpdaterScope::kSystem
            : UpdaterScope::kUser);
  } else if (geteuid() == 0) {
    scope = std::make_optional(UpdaterScope::kSystem);
  } else {
    const std::string app_id = SwitchValue(kCommandProductId);
    if (app_id.empty()) {
      scope = std::make_optional(UpdaterScope::kUser);
    }
  }

  // Never attempt to use the user service as root. If we got flags telling us
  // to do so, bail out. Because Mach service contexts aren't altered by sudo,
  // ksadmin can talk to an already-running user service that, as root, it
  // would not launch.
  if (geteuid() == 0) {
    CHECK(scope) << "ChooseService undetermined for root.";
    if (!IsSystemInstall(*scope)) {
      LOG(ERROR) << "ksadmin cannot use user stores when running as root.";
      Shutdown(1);
      return;
    }
  }
  if (scope) {
    MaybeInstallUpdater(scope.value());
    std::move(callback).Run(scope.value());
  } else {
    system_service_proxy_->GetAppStates(
        base::BindOnce(&KSAdminApp::FinishChoosingServiceWithSystemAppStates,
                       this, std::move(callback)));
  }
}

void KSAdminApp::FinishChoosingServiceWithSystemAppStates(
    base::OnceCallback<void(UpdaterScope)> callback,
    const std::vector<updater::UpdateService::AppState>& states) const {
  std::string app_id = SwitchValue(kCommandProductId);
  for (const updater::UpdateService::AppState& state : states) {
    if (base::EqualsCaseInsensitiveASCII(app_id, state.app_id)) {
      // Found a system app with the right ID; check the path to find out if it
      // is the same installation of the app, or if this must be a user-scope
      // instance in a different location instead.
      @autoreleasepool {
        NSString* state_ecp = base::apple::FilePathToNSString(state.ecp);
        return std::move(callback).Run(MatchesXCPath(state_ecp)
                                           ? UpdaterScope::kSystem
                                           : UpdaterScope::kUser);
      }
    }
  }
  if (!states.size()) {
    // Consider unmigrated Keystone system tickets.
    @autoreleasepool {
      NSDictionary<NSString*, KSTicket*>* store =
          LoadTicketStore(UpdaterScope::kSystem);
      KSTicket* ticket =
          store[[base::SysUTF8ToNSString(app_id) lowercaseString]];
      if (ticket) {
        // Compare the path to find out if this is the same installation.
        UpdaterScope scope = MatchesXCPath(ticket.existenceChecker.path)
                                 ? UpdaterScope::kSystem
                                 : UpdaterScope::kUser;
        MaybeInstallUpdater(scope);
        std::move(callback).Run(scope);
        return;
      }
    }
  }
  // No matching system ticket.
  MaybeInstallUpdater(UpdaterScope::kUser);
  std::move(callback).Run(UpdaterScope::kUser);
}

void KSAdminApp::PrintUsage(const std::string& error_message) {
  if (!error_message.empty()) {
    LOG(ERROR) << error_message;
  }
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
  registration.brand_path = base::FilePath(SwitchValue(kCommandBrandPath));
  registration.version = base::Version(SwitchValue(kCommandVersion));
  registration.existence_checker_path =
      base::FilePath(SwitchValue(kCommandXCPath));
  const std::string brand_key = SwitchValue(kCommandBrandKey);
  if (!brand_key.empty() &&
      brand_key != base::SysNSStringToUTF8(kCRUTicketBrandKey)) {
    LOG(WARNING) << "Ignoring unsupported brand key (use KSBrandID).";
  }

  const std::string tag_key = SwitchValue(kCommandTagKey);
  const std::string tag_path = SwitchValue(kCommandTagPath);
  if (tag_key.empty() != tag_path.empty()) {
    PrintUsage("--tag-key must be set if and only if --tag-path is set.");
    return;
  } else if (!tag_key.empty() && !tag_path.empty()) {
    registration.ap_path = base::FilePath(tag_path);
    registration.ap_key = tag_key;
  }

  const std::string version_key = SwitchValue(kCommandVersionKey);
  const std::string version_path = SwitchValue(kCommandVersionPath);
  if (version_key.empty() != version_path.empty()) {
    PrintUsage(
        "--version-key must be set if and only if --version-path is set.");
    return;
  } else if (!version_key.empty() && !version_path.empty()) {
    registration.version_path = base::FilePath(version_path);
    registration.version_key = version_key;
  }

  if (registration.app_id.empty()) {
    PrintUsage("--register requires -P.");
    return;
  }

  UpdaterScope scope =
      geteuid() == 0 ? UpdaterScope::kSystem : UpdaterScope::kUser;
  MaybeInstallUpdater(scope);
  ServiceProxy(scope)->RegisterApp(
      registration, base::BindOnce(
                        [](base::OnceCallback<void(int)> cb, int result) {
                          if (result == kRegistrationSuccess) {
                            std::move(cb).Run(0);
                          } else {
                            LOG(ERROR)
                                << "Updater registration error: " << result;
                            std::move(cb).Run(1);
                          }
                        },
                        base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::UpdateApp() {
  ChooseService(base::BindOnce(&KSAdminApp::DoUpdateApp, this));
}

void KSAdminApp::DoUpdateApp(UpdaterScope scope) {
  std::string app_id = SwitchValue(kCommandProductId);
  if (app_id.empty()) {
    PrintUsage("productid missing");
    return;
  }

  ServiceProxy(scope)->Update(
      app_id, GetInstallDataIndexFromAppArgs(app_id),
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

void KSAdminApp::ListAppUpdate() {
  ChooseService(base::BindOnce(&KSAdminApp::DoListAppUpdate, this));
}

void KSAdminApp::DoListAppUpdate(UpdaterScope scope) {
  std::string app_id = SwitchValue(kCommandProductId);
  if (app_id.empty()) {
    PrintUsage("productid missing");
    return;
  }

  auto update_check_result = base::MakeRefCounted<UpdateCheckResult>(app_id);
  ServiceProxy(scope)->CheckForUpdate(
      app_id,
      HasSwitch(kCommandUserInitiated) ? UpdateService::Priority::kForeground
                                       : UpdateService::Priority::kBackground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(
          [](scoped_refptr<UpdateCheckResult> update_check_result,
             const UpdateService::UpdateState& update_state) {
            if (update_state.state ==
                UpdateService::UpdateState::State::kUpdateAvailable) {
              update_check_result->set_next_version(
                  update_state.next_version.GetString());
            }
          },
          update_check_result),
      base::BindOnce(
          [](scoped_refptr<UpdateCheckResult> update_check_result,
             base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            if (result == UpdateService::Result::kSuccess) {
              // This output format must not be changed, because the Keystone
              // Registration Framework is expecting the exact format.
              printf("Available updates: (\n");
              if (!update_check_result->next_version().empty()) {
                printf("\t{\n"
                       "\tkServerProductID = \"%s\";\n"
                       "\tkServerDisplayVersion = \"%s\";\n"
                       "\tkServerVersion = \"%s\";\n"
                       "\t}\n",
                       update_check_result->app_id().c_str(),
                       update_check_result->next_version().c_str(),
                       update_check_result->next_version().c_str());
              }
              printf(")\n");
              std::move(cb).Run(0);
            } else {
              LOG(ERROR) << "Error code: " << result;
              std::move(cb).Run(1);
            }
          },
          update_check_result, base::BindOnce(&KSAdminApp::Shutdown, this)));
}

bool KSAdminApp::HasSwitch(const std::string& arg) const {
  return updater::HasSwitch(arg, switches_);
}

std::string KSAdminApp::SwitchValue(const std::string& arg) const {
  return updater::SwitchValue(arg, switches_);
}

void KSAdminApp::Delete() {
  // Existing updater clients may call `ksadmin --delete` to delete an app
  // ticket in one of the following situations:
  // 1) The app is uninstalled. In this case, the path existence checker should
  //    return false. That means the app will be un-registered by the periodic
  //    tasks at certain point.
  // 2) The user updater figures that the app is managed by the system updater
  //    as well. A common scenario is that a second user installed the same app
  //    and then promoted it to a system app. In this case, we can ignore the
  //    deletion request and just rely on the system updater to run update.
  //    The downside is that sometimes the user updater gets the app update
  //    error. But this could an existing problem when two user updaters manage
  //    the same app together.
  // So in summary, we can just omit the ticket deletion request here.
  Shutdown(0);
}

NSDictionary<NSString*, KSTicket*>* KSAdminApp::LoadTicketStore(
    UpdaterScope scope) const {
  return [KSTicketStore readStoreWithPath:base::SysUTF8ToNSString(
                                              KeystoneTicketStorePath(scope))];
}

int KSAdminApp::PrintKeystoneTag(UpdaterScope scope,
                                 const std::string& app_id) const {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = LoadTicketStore(scope);
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
  ChooseService(base::BindOnce(&KSAdminApp::DoPrintTag, this));
}

void KSAdminApp::DoPrintTag(UpdaterScope scope) {
  const std::string app_id = SwitchValue(kCommandProductId);
  if (app_id.empty()) {
    PrintUsage("productid missing");
    return;
  }

  ServiceProxy(scope)->GetAppStates(base::BindOnce(
      [](const std::string& app_id,
         base::OnceCallback<int(const std::string&)> fallback_cb,
         base::OnceCallback<void(int)> done_cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        int exit_code = 0;

        std::vector<updater::UpdateService::AppState>::const_iterator it =
            base::ranges::find_if(
                states,
                [&app_id](const updater::UpdateService::AppState& state) {
                  return base::EqualsCaseInsensitiveASCII(state.app_id, app_id);
                });
        if (it != std::end(states)) {
          KSTicket* ticket = TicketFromAppState(*it);
          printf("%s\n",
                 base::SysNSStringToUTF8([ticket determineTag]).c_str());

        } else {
          // Fallback to print tag from legacy Keystone tickets if there's no
          // matching app registered with the Chromium updater.
          exit_code = std::move(fallback_cb).Run(app_id);
        }

        std::move(done_cb).Run(exit_code);
      },
      app_id, base::BindOnce(&KSAdminApp::PrintKeystoneTag, this, scope),
      base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::PrintVersion() {
  printf("%s\n", kUpdaterVersion);
  Shutdown(0);
}

bool KSAdminApp::PrintKeystoneTickets(UpdaterScope scope,
                                      const std::string& app_id) const {
  // Print all tickets if `app_id` is empty. Otherwise only print ticket for
  // the given app id.
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = LoadTicketStore(scope);
    if (app_id.empty()) {
      if (store.count > 0) {
        for (NSString* key in store) {
          printf("%s\n",
                 base::SysNSStringToUTF8([store[key] description]).c_str());
        }
        return true;
      }
    } else {
      KSTicket* ticket = [store
          objectForKey:[base::SysUTF8ToNSString(app_id) lowercaseString]];
      if (ticket) {
        printf("%s\n", base::SysNSStringToUTF8([ticket description]).c_str());
        return true;
      }
    }

    printf("No tickets.\n");
    return false;
  }
}

void KSAdminApp::PrintTickets() {
  ChooseService(base::BindOnce(&KSAdminApp::DoPrintTickets, this));
}

void KSAdminApp::DoPrintTickets(UpdaterScope scope) {
  const std::string app_id = SwitchValue(kCommandProductId);
  ServiceProxy(scope)->GetAppStates(base::BindOnce(
      [](const std::string& app_id, base::OnceCallback<bool()> fallback_cb,
         base::OnceCallback<void(int)> done_cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        bool ticket_printed = false;
        for (const updater::UpdateService::AppState& state : states) {
          if (!app_id.empty() &&
              !base::EqualsCaseInsensitiveASCII(app_id, state.app_id)) {
            continue;
          }
          KSTicket* ticket = TicketFromAppState(state);
          printf("%s\n", base::SysNSStringToUTF8([ticket description]).c_str());
          ticket_printed = true;
        }

        // Fallback to print legacy Keystone tickets if there's no apps
        // registered with the new updater.
        if (states.empty()) {
          ticket_printed = std::move(fallback_cb).Run();
        }
        std::move(done_cb).Run(ticket_printed || app_id.empty() ? 0 : 1);
      },
      app_id,
      base::BindOnce(&KSAdminApp::PrintKeystoneTickets, this, scope, app_id),
      base::BindOnce(&KSAdminApp::Shutdown, this)));
}

void KSAdminApp::FirstTaskRun() {
  if ((geteuid() == 0) && (getuid() != 0)) {
    if (setuid(0) || setgid(0)) {
      LOG(ERROR) << "Can't setuid()/setgid() appropriately.";
      Shutdown(1);
      return;
    }
  }
  const std::map<std::string, void (KSAdminApp::*)()> commands = {
      {kCommandDelete, &KSAdminApp::Delete},
      {kCommandInstall, &KSAdminApp::UpdateApp},
      {kCommandList, &KSAdminApp::ListAppUpdate},
      {kCommandKsadminVersion, &KSAdminApp::PrintVersion},
      {kCommandPrintTag, &KSAdminApp::PrintTag},
      {kCommandPrintTickets, &KSAdminApp::PrintTickets},
      {kCommandRegister, &KSAdminApp::Register},
  };
  for (const auto& [command, method] : commands) {
    if (HasSwitch(command)) {
      (this->*method)();
      return;
    }
  }

  // Fallbacks to Register: if none of the above exist, these switches signal
  // an intent to register.
  for (const std::string& command :
       {kCommandTag, kCommandTagKey, kCommandTagPath, kCommandBrandKey,
        kCommandBrandPath, kCommandVersion, kCommandVersionKey,
        kCommandVersionPath, kCommandXCPath}) {
    if (HasSwitch(command)) {
      Register();
      return;
    }
  }
  PrintUsage("");
}

}  // namespace

int KSAdminAppMain(int argc, const char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  std::map<std::string, std::string> command_line =
      ParseCommandLine(argc, argv);
  updater::InitLogging(geteuid() ? UpdaterScope::kUser : Scope(command_line));
  InitializeThreadPool("keystone");
  const base::ScopedClosureRunner shutdown_thread_pool(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  // base::CommandLine may reorder arguments and switches, this is not the exact
  // command line.
  VLOG(0) << base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  VLOG(0) << "ksadmin version: " << kUpdaterVersion;

  int exit = base::MakeRefCounted<KSAdminApp>(command_line)->Run();
  VLOG(0) << "Exiting " << exit;
  return exit;
}

}  // namespace updater
